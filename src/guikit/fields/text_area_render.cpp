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
    const keel::f32 effScrollX = extData ? extData->scrollX : 0.0f;
    const keel::f32 effScrollY = extData ? extData->scrollY : scrollY;
    const keel::usize selStart = extData ? extData->selectionStart : cursor;
    const keel::usize selEnd = extData ? extData->selectionEnd : cursor;
    const bool hasSelection = (selEnd > selStart);
    const bool searchOpen = extData && extData->searchOpen;
    const bool replaceOpen = extData && extData->replaceOpen;
    const bool gotoOpen = extData && extData->gotoOpen;
    const keel::ArrayRef<keel::usize> foldedLines = extData ? extData->foldedLines : keel::ArrayRef<keel::usize>{};
    const keel::ArrayRef<keel::usize> breakpoints = extData ? extData->breakpoints : opts.breakpoints.as_ref();

    keel::Array<LogicalLine> fallbackLogicalLines;
    keel::ArrayRef<LogicalLine> logicalLines =
        (extData && !extData->precomputedLogicalLines.empty()) ? extData->precomputedLogicalLines : keel::ArrayRef<LogicalLine>{};
    if (logicalLines.empty()) {
        SplitLogicalLines(text, fallbackLogicalLines);
        logicalLines = fallbackLogicalLines;
    }

    keel::Array<FoldRegion> fallbackFoldRegions;
    keel::ArrayRef<FoldRegion> foldRegions =
        (extData && opts.codeFolding) ? extData->precomputedFoldRegions : keel::ArrayRef<FoldRegion>{};
    if (opts.codeFolding && (!extData || extData->precomputedLogicalLines.empty())) {
        ComputeFoldRegions(text, logicalLines, opts.syntax.has_value() ? &*opts.syntax : nullptr, fallbackFoldRegions);
        foldRegions = fallbackFoldRegions;
    }

    keel::Array<VisualLine> visualLines;
    const EditorLayoutRects L = ComputeEditorLayout(
        bounds, text, logicalLines, opts, size, &canvas, theme,
        effZoom, effScrollX, effScrollY,
        extData ? extData->copyFeedbackActive : false,
        searchOpen, replaceOpen, gotoOpen,
        foldRegions, foldedLines, visualLines,
        extData ? extData->debugToolbarRatio : -1.0f
    );

    const Rect editorBox = L.editorBox;
    if (editorBox.w <= 0.0f || editorBox.h <= 0.0f) return;

    // Render editor container frame via FieldFrame
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

    // Precompute search matches and word occurrence target
    keel::Array<SearchMatch> searchMatches;
    if (searchOpen && extData && !extData->searchQuery.empty()) {
        FindAllSearchMatches(text, logicalLines, extData->searchQuery, extData->matchCase, extData->wholeWord, searchMatches);
    }

    const keel::usize safeCursor = keel::min(cursor, text.size());
    keel::StringRef activeWord{};
    if (opts.highlightOccurrences && focused && !disabled && !searchOpen) {
        if (hasSelection && (selEnd - selStart) <= 48) {
            const keel::StringRef s = text.substr(selStart, selEnd - selStart);
            bool validIdent = !s.empty() && IsIdentStart(s[0]);
            for (char ch : s) {
                if (!IsIdentChar(ch)) { validIdent = false; break; }
            }
            if (validIdent) activeWord = s;
        } else if (!hasSelection) {
            activeWord = ExtractWordAtOffset(text, safeCursor);
        }
    }

    keel::usize matchBracketOpen = keel::StringRef::npos;
    keel::usize matchBracketClose = keel::StringRef::npos;
    const bool hasBracketPair = opts.matchBrackets && focused && !disabled &&
        FindMatchingBracketPair(text, safeCursor, matchBracketOpen, matchBracketClose);

    canvas.PushClip(innerBounds);

    // 3. Line numbers & debugger gutter background
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
    const keel::f32 clampedScrollY = keel::clamp(effScrollY, 0.0f, L.maxScrollY);
    const keel::f32 clampedScrollX = keel::clamp(effScrollX, 0.0f, L.maxScrollX);

    const keel::usize activeVisRow = FindVisualLineForOffset(visualLines, safeCursor);
    const keel::usize activeLogicalLine = visualLines[activeVisRow].logicalLine;

    // 4. Placeholder when empty
    if (text.empty() && !opts.placeholder.empty() && !focused) {
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

    // 5. Virtualized visible lines loop
    const keel::usize totalVisLines = visualLines.size();
    const keel::usize firstVisLine = (L.lineHeight > 0.0f)
        ? keel::as<keel::usize>(keel::max(0.0f, keel::floor(clampedScrollY / L.lineHeight)))
        : 0;
    const keel::usize maxVisCount = (L.lineHeight > 0.0f)
        ? (keel::as<keel::usize>(keel::ceil(textViewport.h / L.lineHeight)) + 2)
        : totalVisLines;
    const keel::usize lastVisLine = keel::min(totalVisLines, firstVisLine + maxVisCount);

    keel::Array<TextSpan> syntaxSpans;
    syntaxSpans.reserve(32);
    keel::usize cachedLogicalLine = keel::as<keel::usize>(-1);

    const bool useCallbackSyntax = (opts.syntaxHighlighter != nullptr);
    const bool useConfigSyntax = (!useCallbackSyntax && opts.syntax.has_value() && opts.syntax->enabled);
    const bool anySyntax = (useCallbackSyntax || useConfigSyntax) && !disabled;

    // Compute multiline block comment state up to the first visible logical line
    bool inBlockComment = false;
    if (useConfigSyntax && !opts.syntax->blockCommentStart.empty() && !opts.syntax->blockCommentEnd.empty() && firstVisLine < totalVisLines) {
        const keel::usize targetLog = visualLines[firstVisLine].logicalLine;
        const keel::usize scanStart = (targetLog > 250) ? (targetLog - 250) : 0;
        for (keel::usize li = scanStart; li < targetLog; ++li) {
            const keel::StringRef ls = text.substr(logicalLines[li].startOffset, logicalLines[li].length);
            keel::usize pos = 0;
            while (pos < ls.size()) {
                if (!inBlockComment) {
                    if (!opts.syntax->lineComment.empty() && ls.substr(pos).starts_with(opts.syntax->lineComment) &&
                        !ls.substr(pos).starts_with(opts.syntax->blockCommentStart)) {
                        break;
                    }
                    const keel::usize bStart = ls.find(opts.syntax->blockCommentStart, pos);
                    if (bStart == keel::StringRef::npos) break;
                    inBlockComment = true;
                    pos = bStart + opts.syntax->blockCommentStart.size();
                } else {
                    const keel::usize bEnd = ls.find(opts.syntax->blockCommentEnd, pos);
                    if (bEnd == keel::StringRef::npos) break;
                    inBlockComment = false;
                    pos = bEnd + opts.syntax->blockCommentEnd.size();
                }
            }
        }
    }

    Rect hoveredWordScreenRect{};

    for (keel::usize vIdx = firstVisLine; vIdx < lastVisLine; ++vIdx) {
        const VisualLine& vl = visualLines[vIdx];
        const keel::f32 lineY = textViewport.y + keel::as<keel::f32>(vIdx) * L.lineHeight - clampedScrollY;
        if (lineY + L.lineHeight < innerBounds.y || lineY > innerBounds.Bottom()) {
            continue;
        }

        const bool isExecLine = (opts.executionLine == vl.logicalLine + 1);

        // Debugger execution line golden highlight OR active cursor line highlight
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

            // Breakpoint indicator (left lane of bpColWidth)
            const bool hasBp = ContainsSizeT(breakpoints, vl.logicalLine + 1);
            const bool hovBp = extData && (extData->hoveredGutterLine == keel::as<keel::i32>(vl.logicalLine));
            if (hasBp || hovBp) {
                const keel::f32 bpR = keel::min(L.lineHeight * 0.28f, 5.0f * effZoom);
                const keel::f32 bpX = innerBounds.x + keel::max(bpR + 3.5f, L.bpColWidth * 0.25f);
                const Color bpCol = hasBp ? Color(239, 68, 68, 245) : Color(239, 68, 68, 105);
                canvas.DrawRoundRect(Rect(bpX - bpR, centerY - bpR, bpR * 2.0f, bpR * 2.0f), bpR, bpCol, hasBp ? Color(254, 202, 202, 180) : Color(0, 0, 0, 0), 1.0f);
            }

            // Debugger execution arrow (â–¶) (right lane of bpColWidth, never overlapping breakpoint)
            if (isExecLine) {
                const keel::f32 ax = innerBounds.x + keel::max(14.5f * effZoom, L.bpColWidth * 0.56f);
                const keel::f32 aw = 7.5f * effZoom;
                const keel::f32 ah = 5.0f * effZoom;
                const Color arrowFill(251, 191, 36, 255);
                const Color arrowStroke(180, 83, 9, 255);
                canvas.DrawTriangle(
                    {ax, centerY - ah},
                    {ax, centerY + ah},
                    {ax + aw, centerY},
                    arrowFill,
                    arrowStroke,
                    1.0f
                );
            }

            // Line number
            if (opts.showLineNumbers) {
                char numBuf[16]{};
                const keel::i32 nLen = std::snprintf(numBuf, sizeof(numBuf), "%zu", vl.logicalLine + 1);
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

            // Code folding chevron (â–¾ / â–¸)
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

        const keel::f32 startX = textViewport.x - clampedScrollX;
        const keel::usize vlEndOffset = vl.startOffset + vl.length;
        const LogicalLine& ll = logicalLines[vl.logicalLine];
        const keel::StringRef fullLineStr = text.substr(ll.startOffset, ll.length);
        const keel::StringRef subLineStr = text.substr(vl.startOffset, vl.length);

        // Indent guides
        const keel::usize tabStep = keel::clamp<keel::usize>(opts.tabSize, 1, 16);
        if (opts.showIndentGuides && vl.isFirstSubLine && !fullLineStr.empty()) {
            keel::usize leadSpaces = 0;
            for (char ch : fullLineStr) {
                if (ch == ' ') ++leadSpaces;
                else if (ch == '\t') leadSpaces += tabStep;
                else break;
            }
            const Color guideCol = hasCustomEditorSurface ? editorBorderSubtle : pal.borderSubtle.WithAlphaF(0.42f);
            for (keel::usize g = tabStep; g < leadSpaces; g += tabStep) {
                const keel::f32 gx = startX + keel::as<keel::f32>(g) * L.metrics.spaceAdvance;
                if (gx >= textViewport.x && gx <= textViewport.Right()) {
                    canvas.DrawLine({gx, lineY}, {gx, lineY + L.lineHeight}, guideCol, 1.0f);
                }
            }
        }

        // Word occurrence highlights on this line
        if (!activeWord.empty() && subLineStr.size() >= activeWord.size()) {
            const keel::usize wLen = activeWord.size();
            for (keel::usize col = 0; col + wLen <= subLineStr.size(); ) {
                if (subLineStr.substr(col, wLen) == activeWord) {
                    const bool leftOk = (col == 0) || !IsIdentChar(subLineStr[col - 1]);
                    const bool rightOk = (col + wLen == subLineStr.size()) || !IsIdentChar(subLineStr[col + wLen]);
                    if (leftOk && rightOk) {
                        const keel::f32 ox = startX + MeasureSegmentWidth(&canvas, subLineStr.substr(0, col), L.fontSize, textStyle, opts.tabSize);
                        const keel::f32 ow = MeasureSegmentWidth(&canvas, activeWord, L.fontSize, textStyle, opts.tabSize);
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

        // Search match highlights on this line
        if (!searchMatches.empty()) {
            for (keel::usize mi = 0; mi < searchMatches.size(); ++mi) {
                const SearchMatch& m = searchMatches[mi];
                if (m.logicalLine != vl.logicalLine) continue;
                if (m.endOffset <= vl.startOffset || m.startOffset >= vlEndOffset) continue;
                const keel::usize s0 = keel::max(m.startOffset, vl.startOffset) - vl.startOffset;
                const keel::usize s1 = keel::min(m.endOffset, vlEndOffset) - vl.startOffset;
                const keel::f32 mx = startX + MeasureSegmentWidth(&canvas, subLineStr.substr(0, s0), L.fontSize, textStyle, opts.tabSize);
                const keel::f32 mw = MeasureSegmentWidth(&canvas, subLineStr.substr(s0, s1 - s0), L.fontSize, textStyle, opts.tabSize);
                const bool isActiveMatch = (extData && mi == extData->activeMatchIdx);
                const Color mFill = isActiveMatch ? Color(245, 158, 11, 120) : Color(245, 158, 11, 58);
                const Color mStroke = isActiveMatch ? Color(251, 191, 36, 240) : Color(245, 158, 11, 140);
                canvas.DrawRoundRect(Rect(mx, lineY + 1.0f, mw, L.lineHeight - 2.0f), 2.5f, mFill, mStroke, 1.0f);
            }
        }

        // Selection highlight rectangle
        if (hasSelection && focused && !disabled) {
            const keel::usize s0 = keel::max(selStart, vl.startOffset);
            const keel::usize s1 = keel::min(selEnd, vlEndOffset);
            if (s1 > s0 || (selStart <= vl.startOffset && selEnd > vlEndOffset)) {
                const keel::StringRef preSel = text.substr(vl.startOffset, s0 - vl.startOffset);
                const keel::StringRef midSel = text.substr(s0, s1 - s0);
                const keel::f32 selX0 = startX + MeasureSegmentWidth(&canvas, preSel, L.fontSize, textStyle, opts.tabSize);
                keel::f32 selW = MeasureSegmentWidth(&canvas, midSel, L.fontSize, textStyle, opts.tabSize);
                if (selEnd > vlEndOffset) {
                    selW += L.metrics.spaceAdvance;
                }
                if (selW > 0.0f) {
                    canvas.DrawRect(Rect(selX0, lineY, selW, L.lineHeight), synPal.selectionBg);
                }
            }
        }

        // Bracket matching boxes
        if (hasBracketPair) {
            const keel::usize bracketOffsets[2] = {matchBracketOpen, matchBracketClose};
            for (keel::usize bOff : bracketOffsets) {
                if (bOff >= vl.startOffset && bOff < vlEndOffset) {
                    const keel::usize bCol = bOff - vl.startOffset;
                    const keel::f32 bx = startX + MeasureSegmentWidth(&canvas, subLineStr.substr(0, bCol), L.fontSize, textStyle, opts.tabSize);
                    const keel::f32 bw = L.metrics.spaceAdvance;
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
            const keel::usize wCol = keel::min(extData->hoveredWordCol, subLineStr.size());
            const keel::f32 wx = startX + MeasureSegmentWidth(&canvas, subLineStr.substr(0, wCol), L.fontSize, textStyle, opts.tabSize);
            const keel::f32 ww = MeasureSegmentWidth(&canvas, extData->hoveredWord, L.fontSize, textStyle, opts.tabSize);
            hoveredWordScreenRect = Rect(wx, lineY, ww, L.lineHeight);
        }

        // Render text or syntax spans
        const Color wsCol = editorTextMuted.WithAlphaF(0.55f);
        const keel::f32 wsCenterY = lineY + L.lineHeight * 0.52f;
        const keel::f32 wsDotR = keel::max(1.1f, 1.35f * effZoom);
        auto drawSpaceDots = [&](keel::f32 baseX, keel::usize count) {
            if (!opts.showWhitespace || count == 0) return;
            for (keel::usize i = 0; i < count; ++i) {
                const keel::f32 dx = baseX + (keel::as<keel::f32>(i) + 0.5f) * L.metrics.spaceAdvance;
                if (dx >= textViewport.x - 2.0f && dx <= textViewport.Right() + 2.0f) {
                    canvas.DrawRoundRect(Rect(dx - wsDotR, wsCenterY - wsDotR, wsDotR * 2.0f, wsDotR * 2.0f), wsDotR, wsCol);
                }
            }
        };
        auto drawTabArrows = [&](keel::f32 baseX, keel::usize count) {
            if (!opts.showWhitespace || count == 0) return;
            const keel::f32 strokeW = keel::max(1.0f, 1.15f * effZoom);
            const keel::f32 headSz = keel::min(L.lineHeight * 0.20f, L.metrics.spaceAdvance * 0.45f);
            for (keel::usize i = 0; i < count; ++i) {
                const keel::f32 tx0 = baseX + keel::as<keel::f32>(i) * L.metrics.tabAdvance + L.metrics.spaceAdvance * 0.35f;
                const keel::f32 tx1 = baseX + keel::as<keel::f32>(i + 1) * L.metrics.tabAdvance - L.metrics.spaceAdvance * 0.35f;
                if (tx1 >= textViewport.x - 2.0f && tx0 <= textViewport.Right() + 2.0f && tx1 > tx0) {
                    canvas.DrawLine({tx0, wsCenterY}, {tx1, wsCenterY}, wsCol, strokeW);
                    canvas.DrawLine({tx1 - headSz, wsCenterY - headSz}, {tx1, wsCenterY}, wsCol, strokeW);
                    canvas.DrawLine({tx1 - headSz, wsCenterY + headSz}, {tx1, wsCenterY}, wsCol, strokeW);
                }
            }
        };

        keel::f32 curX = startX;
        if (vl.length > 0) {
            auto drawPiece = [&](keel::StringRef piece, Color col) {
                keel::usize p = 0;
                while (p < piece.size()) {
                    if (piece[p] == '\t') {
                        keel::usize tabEnd = p + 1;
                        while (tabEnd < piece.size() && piece[tabEnd] == '\t') {
                            ++tabEnd;
                        }
                        const keel::usize tabCount = tabEnd - p;
                        drawTabArrows(curX, tabCount);
                        curX += keel::as<keel::f32>(tabCount) * L.metrics.tabAdvance;
                        p = tabEnd;
                    } else {
                        keel::usize nextTab = piece.find('\t', p);
                        if (nextTab == keel::StringRef::npos) {
                            nextTab = piece.size();
                        }
                        const keel::StringRef seg = piece.substr(p, nextTab - p);

                        keel::usize firstNonSpace = 0;
                        while (firstNonSpace < seg.size() && seg[firstNonSpace] == ' ') {
                            ++firstNonSpace;
                        }
                        if (firstNonSpace > 0) {
                            drawSpaceDots(curX, firstNonSpace);
                            curX += keel::as<keel::f32>(firstNonSpace) * L.metrics.spaceAdvance;
                        }

                        if (firstNonSpace < seg.size()) {
                            keel::usize lastNonSpace = seg.size();
                            while (lastNonSpace > firstNonSpace && seg[lastNonSpace - 1] == ' ') {
                                --lastNonSpace;
                            }
                            const keel::StringRef body = seg.substr(firstNonSpace, lastNonSpace - firstNonSpace);
                            const keel::f32 bodyW = MeasureChunkNoTabs(&canvas, body, L.fontSize, textStyle);
                            if (curX + bodyW >= textViewport.x - 4.0f && curX <= textViewport.Right() + 4.0f) {
                                canvas.DrawTextStyled(body, Vec2{curX, textY}, L.fontSize, col, textStyle);
                                if (opts.showWhitespace) {
                                    for (keel::usize bi = 0; bi < body.size(); ++bi) {
                                        if (body[bi] == ' ') {
                                            const keel::f32 spX = curX + MeasureChunkNoTabs(&canvas, body.substr(0, bi), L.fontSize, textStyle) + L.metrics.spaceAdvance * 0.5f;
                                            canvas.DrawRoundRect(Rect(spX - wsDotR, wsCenterY - wsDotR, wsDotR * 2.0f, wsDotR * 2.0f), wsDotR, wsCol);
                                        }
                                    }
                                }
                            }
                            curX += bodyW;

                            const keel::usize trailingSpaces = seg.size() - lastNonSpace;
                            if (trailingSpaces > 0) {
                                drawSpaceDots(curX, trailingSpaces);
                                curX += keel::as<keel::f32>(trailingSpaces) * L.metrics.spaceAdvance;
                            }
                        }
                        p = nextTab;
                    }
                }
            };

            if (anySyntax) {
                if (cachedLogicalLine != vl.logicalLine) {
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
                    cachedLogicalLine = vl.logicalLine;
                }

                const keel::usize subStartCol = vl.startCol;
                const keel::usize subEndCol = vl.startCol + vl.length;
                keel::usize curCol = subStartCol;

                for (const TextSpan& sp : syntaxSpans) {
                    const keel::usize spStart = sp.startCol;
                    const keel::usize spEnd = sp.startCol + sp.length;
                    if (spEnd <= subStartCol) continue;
                    if (spStart >= subEndCol) break;

                    const keel::usize clipStart = keel::max(spStart, subStartCol);
                    const keel::usize clipEnd = keel::min(spEnd, subEndCol);

                    if (clipStart > curCol) {
                        drawPiece(fullLineStr.substr(curCol, clipStart - curCol), synPal.identifier);
                    }
                    if (clipEnd > clipStart) {
                        drawPiece(fullLineStr.substr(clipStart, clipEnd - clipStart), sp.color);
                        curCol = clipEnd;
                    }
                }
                if (curCol < subEndCol) {
                    drawPiece(fullLineStr.substr(curCol, subEndCol - curCol), synPal.identifier);
                }
            } else {
                const Color textCol = disabled ? pal.textDisabled : editorTextPrimary;
                drawPiece(subLineStr, textCol);
            }
        }

        // Visible EOL return marker (â†µ) when showWhitespace is enabled and this is the end of the logical line
        if (opts.showWhitespace && (vl.startCol + vl.length == ll.length) && (vl.logicalLine + 1 < logicalLines.size())) {
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

        // Folded block summary pill "[...]" at end of line
        const bool isFoldedStart = opts.codeFolding && vl.isFirstSubLine && IsLineFoldedStart(foldedLines, vl.logicalLine);
        if (isFoldedStart) {
            const keel::f32 pillX = curX + L.metrics.spaceAdvance * 0.8f;
            const keel::f32 pillW = L.metrics.spaceAdvance * 4.2f;
            const keel::f32 pillH = L.lineHeight - 4.0f;
            const Rect pillRect(pillX, lineY + 2.0f, pillW, pillH);
            canvas.DrawRoundRect(pillRect, 3.0f, editorGutterBg, editorBorderSubtle, 1.0f);
            canvas.DrawTextBox("...", pillRect, L.fontSize * 0.9f, editorTextSecondary, 0.5f, 0.45f);
            curX = pillRect.Right();
        }

        // Inline Debugger Variable Readouts (e.g. "N = 1024 | peak = +18.4 dB")
        // Attached to the end of the logical line (last visual sub-line, or folded block header)
        const bool isLastSubLine = (vl.startCol + vl.length >= ll.length) || isFoldedStart;
        if (isLastSubLine && !opts.inlineValues.empty()) {
            const keel::StringRef cmtPrefix = (opts.syntax.has_value() && !opts.syntax->lineComment.empty())
                ? opts.syntax->lineComment
                : "//";
            if (!ExtractCodeWithoutLineComment(fullLineStr, cmtPrefix).empty()) {
                for (const CodeInlineValue& iv : opts.inlineValues) {
                    if (iv.line == vl.logicalLine + 1 && !iv.text.empty()) {
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
                        if (clampedRight) {
                            canvas.DrawRoundRect(ivRect, 3.5f, editorBgCol.WithAlphaF(0.92f), Color::Transparent(), 0.0f);
                        }
                        canvas.DrawRoundRect(ivRect, 3.5f, accent.WithAlphaF(0.14f), accent.WithAlphaF(0.34f), 1.0f);
                        canvas.DrawTextStyled(iv.text, Vec2{ivX + 6.0f, textY + 0.8f}, ivFs, accent.WithAlphaF(0.94f), FontStyle::Monospace);
                        curX = ivRect.Right();
                    }
                }
            }
        }

        // Diagnostic wavy underline squiggles (~~~)
        if (!opts.diagnostics.empty()) {
            for (const CodeDiagnostic& diag : opts.diagnostics) {
                if (diag.line != vl.logicalLine + 1) continue;
                keel::usize dStart = diag.startCol;
                keel::usize dEnd = (diag.endCol > diag.startCol) ? keel::min<keel::usize>(diag.endCol, fullLineStr.size()) : fullLineStr.size();
                if (dStart == 0 && diag.endCol == 0) {
                    while (dStart < fullLineStr.size() && (fullLineStr[dStart] == ' ' || fullLineStr[dStart] == '\t')) ++dStart;
                }
                if (dEnd <= vl.startCol || dStart >= vl.startCol + vl.length) continue;
                const keel::usize s0 = keel::max(dStart, vl.startCol) - vl.startCol;
                const keel::usize s1 = keel::min(dEnd, vl.startCol + vl.length) - vl.startCol;
                const keel::f32 x0 = startX + MeasureSegmentWidth(&canvas, subLineStr.substr(0, s0), L.fontSize, textStyle, opts.tabSize);
                const keel::f32 x1 = startX + MeasureSegmentWidth(&canvas, subLineStr.substr(0, s1), L.fontSize, textStyle, opts.tabSize);
                const Color sqCol = (diag.severity == DiagnosticSeverity::Error) ? Color(239, 68, 68, 235)
                                  : (diag.severity == DiagnosticSeverity::Warning) ? Color(245, 158, 11, 235)
                                  : Color(56, 189, 248, 220);
                DrawWavyUnderline(canvas, x0, keel::max(x0 + L.metrics.spaceAdvance, x1), lineY + L.lineHeight - 2.0f, sqCol, effZoom);
            }
        }

        // Render active cursor caret (when focus is in the code editor)
        if (focused && cursorVisible && !disabled && vIdx == activeVisRow && (!extData || extData->overlayFocus == 0)) {
            const keel::usize colInSub = (safeCursor >= vl.startOffset)
                ? keel::min(safeCursor - vl.startOffset, vl.length)
                : 0;
            const keel::StringRef beforeCursor = text.substr(vl.startOffset, colInSub);
            const keel::f32 caretX = startX + MeasureSegmentWidth(&canvas, beforeCursor, L.fontSize, textStyle, opts.tabSize);
            if (caretX >= textViewport.x - 2.0f && caretX <= textViewport.Right() + 2.0f) {
                const keel::f32 caretH = keel::max(L.glyphBoxH, L.lineHeight - 2.0f);
                const keel::f32 caretY = lineY + (L.lineHeight - caretH) * 0.5f;
                const keel::f32 reqCaretW = extData ? extData->cursorWidth : 1.8f;
                const keel::f32 caretW = keel::max(1.0f, reqCaretW);
                canvas.DrawRect(
                    Rect(caretX, caretY, caretW, caretH),
                    editorCaret
                );
            }
        }

        canvas.PopClip();
    }

    // 6. Render Right-Hand Code Minimap
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
        const keel::usize startVis = (visualLines.size() > maxMiniLines && L.maxScrollY > 0.0f)
            ? keel::as<keel::usize>((clampedScrollY / L.maxScrollY) * keel::as<keel::f32>(visualLines.size() - maxMiniLines))
            : 0;
        const keel::usize endVis = keel::min(visualLines.size(), startVis + maxMiniLines);

        // Draw viewport slider box on minimap
        const keel::f32 visCountF = keel::as<keel::f32>(keel::max<keel::usize>(1, lastVisLine - firstVisLine));
        const keel::f32 vpTopY = L.minimapRect.y + keel::as<keel::f32>(firstVisLine > startVis ? firstVisLine - startVis : 0) * microStep;
        const keel::f32 vpH = keel::min(L.minimapRect.Bottom() - vpTopY, visCountF * microStep);
        const bool miniActive = extData && (extData->minimapHovered || extData->minimapDragging);
        canvas.DrawRect(
            Rect(L.minimapRect.x + 1.0f, vpTopY, L.minimapRect.w - 1.0f, keel::max(10.0f, vpH)),
            pal.focusRing.WithAlphaF(miniActive ? 0.22f : 0.12f)
        );

        const keel::f32 charScale = (L.minimapRect.w - 8.0f) / 90.0f;
        for (keel::usize mv = startVis; mv < endVis; ++mv) {
            const VisualLine& vl = visualLines[mv];
            const keel::f32 my = L.minimapRect.y + keel::as<keel::f32>(mv - startVis) * microStep + 2.0f;
            const keel::StringRef ls = text.substr(vl.startOffset, vl.length);

            // Execution line marker on minimap
            if (opts.executionLine == vl.logicalLine + 1) {
                canvas.DrawRect(Rect(L.minimapRect.x + 1.0f, my - 0.5f, L.minimapRect.w - 2.0f, microStep), Color(251, 191, 36, 160));
            }
            // Breakpoint dot on minimap
            if (ContainsSizeT(breakpoints, vl.logicalLine + 1)) {
                canvas.DrawRect(Rect(L.minimapRect.x + 2.0f, my, 3.5f, microStep), Color(239, 68, 68, 230));
            }

            // Draw micro-tokens
            keel::usize cIdx = 0;
            while (cIdx < ls.size() && cIdx < 88) {
                while (cIdx < ls.size() && (ls[cIdx] == ' ' || ls[cIdx] == '\t')) ++cIdx;
                if (cIdx >= ls.size() || cIdx >= 88) break;
                const keel::usize tokStart = cIdx;
                while (cIdx < ls.size() && ls[cIdx] != ' ' && ls[cIdx] != '\t' && cIdx < 88) ++cIdx;
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

    // 7. Render vertical & horizontal scrollbars (with overview ruler markers for search/breakpoints/diagnostics)
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

        // Overview ruler ticks on vertical scrollbar track
        const keel::f32 totalLinesF = keel::as<keel::f32>(keel::max<keel::usize>(1, logicalLines.size()));
        for (const SearchMatch& m : searchMatches) {
            const keel::f32 ty = L.vTrackRect.y + (keel::as<keel::f32>(m.logicalLine) / totalLinesF) * L.vTrackRect.h;
            canvas.DrawRect(Rect(L.vTrackRect.x, ty, L.vTrackRect.w, 2.0f), Color(245, 158, 11, 210));
        }
        for (keel::usize bp : breakpoints) {
            if (bp >= 1 && bp <= logicalLines.size()) {
                const keel::f32 ty = L.vTrackRect.y + (keel::as<keel::f32>(bp - 1) / totalLinesF) * L.vTrackRect.h;
                canvas.DrawRect(Rect(L.vTrackRect.x, ty, L.vTrackRect.w, 2.5f), Color(239, 68, 68, 230));
            }
        }
        for (const CodeDiagnostic& d : opts.diagnostics) {
            if (d.line >= 1 && d.line <= logicalLines.size()) {
                const keel::f32 ty = L.vTrackRect.y + (keel::as<keel::f32>(d.line - 1) / totalLinesF) * L.vTrackRect.h;
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

    // 8. Render Debug Hover DataTip Card when hovering over a variable in opts.debugVariables
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
                canvas.DrawRoundRect(cardRect, 5.0f, pal.surfaceElevated, pal.focusRing, 1.2f);
                canvas.DrawTextStyled(headerStr, {cardX + 8.0f, cardY + 4.0f}, tipFs, synPal.type, FontStyle::Monospace);
                canvas.DrawTextStyled(dv.value, {cardX + 8.0f, cardY + 20.0f}, tipFs, pal.textPrimary, FontStyle::Monospace);
                break;
            }
        }
    }

    // 9. Render Floating Debug Toolbar (Continue, Step Over, Step Into, Step Out, Restart, Stop) â€” via UiSize & Button::Render
    const UiSize tbSize = L.toolbarSize;
    const ComponentMetrics& tbCm = theme.GetMetrics(tbSize);
    const keel::f32 badgeFs = tbCm.companionFontSize;
    const keel::f32 btnRad = tbCm.cornerRadius;
    if (opts.showDebugToolbar && L.debugToolbar.panelRect.w > 0.0f) {
        const auto& DT = L.debugToolbar;
        const bool gripDragging = extData && extData->debugToolbarDragging;
        const bool gripHovered = extData && (extData->hoveredDebugBtn == 98 || gripDragging);
        const Color dtBorderCol = gripDragging ? pal.focusRing : (gripHovered ? pal.textSecondary : pal.borderHover);
        canvas.DrawRoundRect(DT.panelRect, btnRad + 3.0f, pal.surfaceElevated.WithAlphaF(0.97f), dtBorderCol, gripDragging ? 1.5f : 1.2f);

        // Draw VS Code-style 6-dot drag grip on the left side of the floating debug toolbar
        const keel::f32 gripSlotW = DT.btnRects[0].x - DT.panelRect.x;
        const keel::f32 dotSz = keel::max(2.0f, tbCm.iconSize * 0.14f);
        const keel::f32 dotStepY = tbCm.iconSize * 0.32f;
        const keel::f32 dotStepX = tbCm.iconSize * 0.28f;
        const keel::f32 gripX = DT.panelRect.x + (gripSlotW - dotStepX - dotSz) * 0.5f;
        const keel::f32 gripCy = DT.panelRect.y + DT.panelRect.h * 0.5f;
        const Color gripCol = gripDragging ? pal.focusRing : (gripHovered ? pal.textPrimary : pal.textMuted.WithAlphaF(0.75f));
        for (keel::i32 rowIdx = -1; rowIdx <= 1; ++rowIdx) {
            const keel::f32 dy = gripCy + keel::as<keel::f32>(rowIdx) * dotStepY - dotSz * 0.5f;
            canvas.DrawRoundRect(Rect(gripX, dy, dotSz, dotSz), dotSz * 0.5f, gripCol);
            canvas.DrawRoundRect(Rect(gripX + dotStepX, dy, dotSz, dotSz), dotSz * 0.5f, gripCol);
        }

        // Draw vertical divider between Step Out (btn 3) and Restart (btn 4) via Toolbar::RenderSeparator
        const Rect sepRect(
            DT.btnRects[3].Right(),
            DT.panelRect.y,
            keel::max(1.0f, DT.btnRects[4].x - DT.btnRects[3].Right()),
            DT.panelRect.h
        );
        detail::Toolbar::RenderSeparator(canvas, sepRect, pal.borderSubtle, theme);

        static constexpr keel::StaticArray<IconId, 6> kDbgIcons = {
            IconId::DebugContinue, // Continue (F5)
            IconId::StepOver,      // Step Over (F10)
            IconId::StepInto,      // Step Into (F11)
            IconId::StepOut,       // Step Out (Shift+F11)
            IconId::Refresh,       // Restart
            IconId::Stop           // Stop (Shift+F5)
        };
        const keel::StaticArray<Color, 6> dbgColors = {
            Color(34, 197, 94),   // Continue (F5) - Emerald
            Color(56, 189, 248),  // Step Over (F10) - Sky Blue
            Color(56, 189, 248),  // Step Into (F11) - Sky Blue
            Color(56, 189, 248),  // Step Out (Shift+F11) - Sky Blue
            Color(34, 197, 94),   // Restart - Emerald/Green (VS Code style)
            Color(239, 68, 68)    // Stop (Shift+F5) - Red
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

    // 10. Render Floating Find & Replace / Go to Line Overlay — via UiSize & Button::Render
    if ((searchOpen || gotoOpen) && L.searchOverlay.panelRect.w > 0.0f && extData) {
        const auto& S = L.searchOverlay;
        canvas.DrawRoundRect(S.panelRect, btnRad + 2.0f, pal.surfaceElevated.WithAlphaF(0.97f), pal.borderHover, 1.0f);

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
            canvas.DrawRoundRect(S.findInputRect, btnRad, pal.inputBackground, findFocused ? pal.focusRing : pal.inputBorder, 1.0f);
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

            // Match Case ("Aa") square toggle button
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
            // Match Whole Word ("ab" with bottom bracket underline) square toggle button
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
            } else {
                std::snprintf(cntBuf, sizeof(cntBuf), "%zu/%zu", extData->activeMatchIdx + 1, searchMatches.size());
            }
            const Color cntCol = (!extData->searchQuery.empty() && searchMatches.empty())
                ? theme.GetTextColor(UiVariant::Danger)
                : pal.textSecondary;
            canvas.DrawTextBox(cntBuf, S.countRect, badgeFs, cntCol, 0.5f, 0.5f);

            // Previous Match (ArrowUp), Next Match (ArrowDown), Close (Close)
            renderSquareToolBtn(S.prevBtnRect, IconId::ArrowUp, false, 3);
            renderSquareToolBtn(S.nextBtnRect, IconId::ArrowDown, false, 4);
            renderSquareToolBtn(S.closeBtnRect, IconId::Close, false, 5);

            if (replaceOpen && S.replaceInputRect.w > 0.0f) {
                const bool repFocused = (extData->overlayFocus == 2);
                canvas.DrawRoundRect(S.replaceInputRect, btnRad, pal.inputBackground, repFocused ? pal.focusRing : pal.inputBorder, 1.0f);
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
                // Replace One & Replace All buttons via Button::Render
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
            canvas.DrawRoundRect(S.findInputRect, btnRad, pal.inputBackground, gotoFocused ? pal.focusRing : pal.inputBorder, 1.0f);
            const Rect gTextRect(S.findInputRect.x + inputPadX, S.findInputRect.y, S.findInputRect.w - inputPadX * 2.0f, S.findInputRect.h);
            if (extData->gotoQuery.empty()) {
                char ph[48]{};
                std::snprintf(ph, sizeof(ph), "Go to Line (1..%zu)...", logicalLines.size());
                canvas.DrawTextBox(ph, gTextRect, badgeFs, pal.textMuted, 0.0f, 0.5f);
            } else {
                canvas.DrawTextBox(extData->gotoQuery, gTextRect, badgeFs, pal.textPrimary, 0.0f, 0.5f);
            }
            renderSquareToolBtn(S.closeBtnRect, IconId::Close, false, 5);
        }
    }

    // 11. Render top-right quick overlays: Copy button & Zoom % reset button — via UiSize & Button::Render
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

    // 12. Render IDE Bottom Status Bar
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
        const keel::usize curLine1 = activeLogicalLine + 1;
        const keel::usize curCol1 = (safeCursor >= logicalLines[activeLogicalLine].startOffset)
            ? (safeCursor - logicalLines[activeLogicalLine].startOffset + 1)
            : 1;

        keel::usize errCnt = 0;
        keel::usize warnCnt = 0;
        for (const CodeDiagnostic& d : opts.diagnostics) {
            if (d.severity == DiagnosticSeverity::Error) ++errCnt;
            else if (d.severity == DiagnosticSeverity::Warning) ++warnCnt;
        }

        keel::f32 sbX = L.statusBarRect.x + 8.0f;
        const keel::f32 sbCy = L.statusBarRect.y + L.statusBarRect.h * 0.5f;
        const keel::f32 sbIcoSz = 11.0f;

        // Debugger status indicator icon + label
        if (opts.executionLine > 0) {
            const Color pauseCol(251, 191, 36);
            Icon::Pause().Draw(canvas, Rect(sbX, sbCy - sbIcoSz * 0.5f, sbIcoSz, sbIcoSz), pauseCol);
            sbX += sbIcoSz + 4.0f;
            char dbgBuf[48]{};
            std::snprintf(dbgBuf, sizeof(dbgBuf), "PAUSED Ln %zu  |", opts.executionLine);
            const keel::f32 dw = TextUtil::MeasureWidth(dbgBuf, sbFs, &canvas);
            canvas.DrawTextBox(dbgBuf, Rect(sbX, L.statusBarRect.y, dw + 6.0f, L.statusBarRect.h), sbFs, pauseCol, 0.0f, 0.5f);
            sbX += dw + 8.0f;
        }

        // Error & Warning icons + counts
        const Color errCol = (errCnt > 0) ? Color(239, 68, 68) : editorTextSecondary;
        Icon::Error().Draw(canvas, Rect(sbX, sbCy - sbIcoSz * 0.5f, sbIcoSz, sbIcoSz), errCol);
        sbX += sbIcoSz + 3.0f;
        char errBuf[16]{};
        std::snprintf(errBuf, sizeof(errBuf), "%zu", errCnt);
        const keel::f32 ew = TextUtil::MeasureWidth(errBuf, sbFs, &canvas);
        canvas.DrawTextBox(errBuf, Rect(sbX, L.statusBarRect.y, ew + 4.0f, L.statusBarRect.h), sbFs, errCol, 0.0f, 0.5f);
        sbX += ew + 8.0f;

        const Color warnCol = (warnCnt > 0) ? Color(245, 158, 11) : editorTextSecondary;
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
                          curLine1, curCol1, selEnd - selStart, logicalLines.size(), breakpoints.size());
        } else {
            std::snprintf(leftStatus, sizeof(leftStatus), "|  Ln %zu, Col %zu  |  %zu lines  |  %zu BPs",
                          curLine1, curCol1, logicalLines.size(), breakpoints.size());
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
        std::snprintf(rightStatus, sizeof(rightStatus), "Spaces: 4  |  UTF-8  |  %d%%  |  %.*s",
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
