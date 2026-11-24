#include "markdown_internal.hpp"

namespace guikit {

using namespace detail::md_impl;

namespace {

keel::u64 g_lastAnyMarkdownRunSeq = 0;

// --- Phase 3A: O(1) / O(K) Document Hit-Testing (Zero Layout, Zero Font Measurement) ---

MarkdownPassResult HitTestMarkdownLayout(
    const MarkdownDocumentLayout& layout,
    Rect outerBounds,
    Vec2 screenMousePos,
    keel::i32 activeCopiedBlock,
    bool interactiveTasks,
    bool showCopyButton,
    bool disabled,
    const keel::HashMap<keel::i32, bool>* taskOverrides
) noexcept {
    MarkdownPassResult out{};
    out.totalWidth = layout.totalWidth;
    out.totalHeight = layout.totalHeight;
    if (disabled || !outerBounds.Contains(screenMousePos)) {
        return out;
    }

    const Vec2 docOrigin = outerBounds.Pos();
    const Vec2 localMouse = screenMousePos - docOrigin;
    const keel::f32 s = layout.scale;

    for (const LinkHitRegion& lr : layout.linkRegions) {
        if (lr.localRect.Contains(localMouse)) {
            out.hoveredLink = lr.url;
            out.hoveredLinkRect = OffsetRect(lr.localRect, docOrigin);
            break;
        }
    }

    if (showCopyButton) {
        for (const CopyBtnHitRegion& cr : layout.copyRegions) {
            const keel::f32 copyBtnW = (activeCopiedBlock == cr.codeBlockIndex) ? (72.0f * s) : (56.0f * s);
            const keel::f32 copyBtnH = 20.0f * s;
            const Rect btnLocalRect(
                cr.boxLocalRect.Right() - copyBtnW - 6.0f * s,
                cr.boxLocalRect.y + (cr.headerH - copyBtnH) * 0.5f,
                copyBtnW,
                copyBtnH
            );
            if (btnLocalRect.Contains(localMouse)) {
                out.hoveredCopyBlock = cr.codeBlockIndex;
                out.hoveredCopyCode = cr.rawCode;
                break;
            }
        }
    }

    if (interactiveTasks) {
        for (const TaskHitRegion& tr : layout.taskRegions) {
            if (tr.localHitRect.Contains(localMouse)) {
                bool isChecked = tr.defaultChecked;
                if (taskOverrides) {
                    if (const bool* ov = taskOverrides->get(tr.taskIndex)) {
                        isChecked = *ov;
                    }
                }
                out.hoveredTaskIndex = tr.taskIndex;
                out.hoveredTaskCurrentState = isChecked;
                break;
            }
        }
    }

    return out;
}

// --- Phase 3B: Zero-Layout Document Rendering with Viewport Frustum Culling ---

void RenderMarkdownDocumentLayout(
    ICanvas& canvas,
    const ParsedMarkdownDoc& doc,
    const MarkdownDocumentLayout& layout,
    Rect outerBounds,
    Rect visibleViewportRect,
    const MarkdownOptions& opts,
    const UiTheme& theme,
    Vec2 screenMousePos,
    keel::i32 hoveredCopyBlock,
    keel::i32 activeCopiedBlock,
    keel::i32 hoveredTaskIndex,
    const RenderOverlayState* overlay
) {
    const Vec2 docOrigin = outerBounds.Pos();
    const Vec2 localMouse = screenMousePos - docOrigin;
    const keel::f32 effZoom = layout.effZoom;
    const keel::f32 s = layout.scale;
    const keel::f32 bodyFs = (opts.bodyFontSize.has_value() ? *opts.bodyFontSize : theme.GetFontSize(TextRole::BodyMedium)) * effZoom;

    for (const BlockLayout& bl : layout.blocks) {
        if (bl.astBlockIndex >= doc.blocks.size()) continue;
        const MarkdownBlock& blk = doc.blocks[bl.astBlockIndex];
        const Rect screenBlk = OffsetRect(bl.localBounds, docOrigin);

        // O(1) Viewport Frustum Culling: skip any block completely outside the visible scroll viewport
        if (visibleViewportRect.h > 0.0f &&
            (screenBlk.Bottom() < visibleViewportRect.y - 2.0f || screenBlk.y > visibleViewportRect.Bottom() + 2.0f)) {
            continue;
        }

        switch (bl.kind) {
            case BlockKind::Divider: {
                const keel::f32 ly = screenBlk.y + screenBlk.h * 0.5f;
                canvas.DrawLine({screenBlk.x, ly}, {screenBlk.Right(), ly}, theme.palette.borderSubtle, 1.0f);
                break;
            }

            case BlockKind::Heading: {
                RenderInlineFlowLayout(
                    canvas,
                    bl.flow,
                    docOrigin + bl.flowLocalOrigin,
                    docOrigin,
                    theme.palette.textPrimary,
                    &layout.textMap,
                    overlay,
                    theme,
                    localMouse
                );
                if (bl.hasHeadingDivider) {
                    const keel::f32 dy = docOrigin.y + bl.dividerLocalY;
                    canvas.DrawLine({screenBlk.x, dy}, {screenBlk.Right(), dy}, theme.palette.borderSubtle, 1.0f);
                }
                break;
            }

            case BlockKind::Paragraph: {
                RenderInlineFlowLayout(
                    canvas,
                    bl.flow,
                    docOrigin + bl.flowLocalOrigin,
                    docOrigin,
                    theme.palette.textPrimary,
                    &layout.textMap,
                    overlay,
                    theme,
                    localMouse
                );
                break;
            }

            case BlockKind::Blockquote: {
                canvas.DrawRoundRect(screenBlk, bl.boxRadius, theme.palette.surfaceElevated.WithAlpha(140));
                canvas.DrawRoundRect(Rect(screenBlk.x, screenBlk.y, 4.0f * s, screenBlk.h), 2.0f * s, theme.palette.borderHover);
                RenderInlineFlowLayout(
                    canvas,
                    bl.flow,
                    docOrigin + bl.flowLocalOrigin,
                    docOrigin,
                    theme.palette.textSecondary,
                    &layout.textMap,
                    overlay,
                    theme,
                    localMouse
                );
                break;
            }

            case BlockKind::Alert: {
                const Color accent = AlertAccentColor(blk.alertKind, theme);
                canvas.DrawRoundRect(screenBlk, bl.boxRadius, accent.WithAlpha(24), accent.WithAlpha(90), 1.0f);
                canvas.DrawRoundRect(Rect(screenBlk.x, screenBlk.y, 4.0f * s, screenBlk.h), 2.0f * s, accent);

                const Icon ic = AlertIcon(blk.alertKind);
                const keel::f32 icSz = 15.0f * s;
                ic.Render(canvas, Rect(screenBlk.x + bl.padX, screenBlk.y + bl.padY + (bl.headerH - icSz) * 0.5f, icSz, icSz), accent);

                const keel::f32 titleFs = theme.GetFontSize(TextRole::LabelLarge) * effZoom;
                canvas.DrawTextStyled(
                    blk.alertTitle,
                    {screenBlk.x + bl.padX + icSz + 8.0f * s, screenBlk.y + bl.padY + (bl.headerH - titleFs) * 0.5f},
                    titleFs,
                    accent,
                    FontStyle::Bold
                );

                RenderInlineFlowLayout(
                    canvas,
                    bl.flow,
                    docOrigin + bl.flowLocalOrigin,
                    docOrigin,
                    theme.palette.textPrimary,
                    &layout.textMap,
                    overlay,
                    theme,
                    localMouse
                );
                break;
            }

            case BlockKind::ListItem: {
                bool isTaskChecked = (blk.taskState == 1);
                if (blk.taskIndex >= 0 && overlay && overlay->taskOverrides) {
                    if (const bool* ov = overlay->taskOverrides->get(blk.taskIndex)) {
                        isTaskChecked = *ov;
                    }
                }

                const Rect markerRect = OffsetRect(bl.markerLocalRect, docOrigin);
                if (blk.taskState >= 0) {
                    const bool cbHovered = (hoveredTaskIndex == blk.taskIndex && blk.taskIndex >= 0);
                    if (isTaskChecked) {
                        canvas.DrawRoundRect(markerRect, 3.0f * s, theme.palette.accent);
                        Icon::Check().Render(canvas, markerRect.Inset(2.0f * s), Color(255, 255, 255, 255));
                    } else {
                        canvas.DrawRoundRect(
                            markerRect,
                            3.0f * s,
                            theme.palette.surfaceElevated,
                            cbHovered ? theme.palette.accent : theme.palette.borderHover,
                            cbHovered ? 1.5f : 1.2f
                        );
                    }
                } else if (blk.isOrderedList) {
                    char numBuf[16];
                    const keel::StringRef numStr = keel::format_buf(numBuf, "{}.", blk.listNumber);
                    canvas.DrawTextStyled(
                        numStr,
                        markerRect.Pos(),
                        bodyFs,
                        theme.palette.accent,
                        FontStyle::Bold
                    );
                } else {
                    canvas.DrawRoundRect(markerRect, markerRect.w * 0.5f, theme.palette.accent);
                }

                RenderInlineFlowLayout(
                    canvas,
                    bl.flow,
                    docOrigin + bl.flowLocalOrigin,
                    docOrigin,
                    isTaskChecked ? theme.palette.textSecondary : theme.palette.textPrimary,
                    &layout.textMap,
                    overlay,
                    theme,
                    localMouse
                );
                break;
            }

            case BlockKind::CodeBlock: {
                const bool hasCustomBg = opts.codePalette.has_value() && opts.codePalette->editorBg.a > 0;
                const Color codeBg = hasCustomBg ? opts.codePalette->editorBg : theme.palette.surfaceElevated;
                const Color gutterBg = (opts.codePalette.has_value() && opts.codePalette->gutterBg.a > 0)
                    ? opts.codePalette->gutterBg
                    : theme.palette.surface;
                const Color defaultCodeCol = opts.codePalette.has_value()
                    ? opts.codePalette->identifier
                    : theme.palette.textPrimary;
                const Color lineNumCol = opts.codePalette.has_value()
                    ? opts.codePalette->comment
                    : theme.palette.textMuted;
                const Color wsColor = opts.codePalette.has_value()
                    ? opts.codePalette->comment.WithAlpha(110)
                    : theme.palette.textMuted.WithAlpha(110);

                canvas.DrawRoundRect(screenBlk, bl.boxRadius, codeBg, theme.palette.borderSubtle, 1.0f);
                canvas.DrawRoundRect(Rect(screenBlk.x, screenBlk.y, screenBlk.w, bl.headerH), bl.boxRadius, gutterBg);
                canvas.DrawLine(
                    {screenBlk.x, screenBlk.y + bl.headerH},
                    {screenBlk.Right(), screenBlk.y + bl.headerH},
                    theme.palette.borderSubtle,
                    1.0f
                );

                const keel::f32 lblFs = theme.GetFontSize(TextRole::LabelSmall) * effZoom;
                const keel::StringRef langLabel = blk.codeLang.empty() ? keel::StringRef("code") : blk.codeLang.as_ref();
                canvas.DrawTextStyled(
                    langLabel,
                    {screenBlk.x + bl.padX, screenBlk.y + (bl.headerH - lblFs) * 0.5f},
                    lblFs,
                    lineNumCol,
                    FontStyle::Bold
                );

                if (opts.showCopyButton) {
                    const bool isHov = (hoveredCopyBlock == blk.codeBlockIndex);
                    const bool isCopied = (activeCopiedBlock == blk.codeBlockIndex);
                    const keel::f32 copyBtnW = isCopied ? (72.0f * s) : (56.0f * s);
                    const keel::f32 copyBtnH = 20.0f * s;
                    const Rect copyBtnRect(
                        screenBlk.Right() - copyBtnW - 6.0f * s,
                        screenBlk.y + (bl.headerH - copyBtnH) * 0.5f,
                        copyBtnW,
                        copyBtnH
                    );
                    const Color btnBg = isCopied
                        ? theme.GetVariant(UiVariant::Success).bg.WithAlpha(40)
                        : (isHov ? theme.palette.accent.WithAlpha(35) : codeBg);
                    const Color btnBorder = isCopied
                        ? theme.GetVariant(UiVariant::Success).bg
                        : (isHov ? theme.palette.accent : theme.palette.borderSubtle);
                    const Color btnText = isCopied
                        ? theme.GetVariant(UiVariant::Success).bg
                        : (isHov ? theme.palette.accent : defaultCodeCol);
                    canvas.DrawRoundRect(copyBtnRect, 4.0f * s, btnBg, btnBorder, 1.0f);
                    canvas.DrawTextBox(
                        isCopied ? "Copied" : "Copy",
                        copyBtnRect,
                        lblFs,
                        btnText,
                        0.5f,
                        0.5f
                    );
                }

                canvas.PushClip(Rect(screenBlk.x + 1.0f, screenBlk.y + bl.headerH + 1.0f, screenBlk.w - 2.0f, bl.bodyH - 2.0f));
                if (opts.codeLineNumbers) {
                    canvas.DrawRect(Rect(screenBlk.x, screenBlk.y + bl.headerH, bl.gutterW, bl.bodyH), gutterBg.WithAlpha(180));
                    canvas.DrawLine(
                        {screenBlk.x + bl.gutterW, screenBlk.y + bl.headerH},
                        {screenBlk.x + bl.gutterW, screenBlk.Bottom()},
                        theme.palette.borderSubtle,
                        1.0f
                    );
                }

                const keel::f32 screenCodeStartX = docOrigin.x + bl.codeStartX;
                const keel::f32 lineNumFs = theme.GetFontSize(TextRole::CodeSmall) * effZoom;

                for (const CodeVisualLineLayout& vline : bl.codeVisualLines) {
                    const keel::f32 screenLy = docOrigin.y + vline.localY;
                    const keel::f32 screenTextY = docOrigin.y + vline.textLocalY;

                    if (opts.codeLineNumbers && vline.isFirstSlice) {
                        char lnBuf[16];
                        std::snprintf(lnBuf, sizeof(lnBuf), "%zu", vline.logicalLine + 1);
                        canvas.DrawTextBox(
                            lnBuf,
                            Rect(screenBlk.x + 4.0f * s, screenLy, bl.gutterW - 10.0f * s, bl.codeLineH),
                            lineNumFs,
                            lineNumCol,
                            1.0f,
                            0.5f
                        );
                    }

                    const PositionedToken* tok = (vline.tokenIndex < layout.textMap.tokens.size())
                        ? &layout.textMap.tokens[vline.tokenIndex]
                        : nullptr;
                    if (tok) {
                        DrawTokenSelectionAndSearchHighlights(canvas, *tok, layout.textMap.plainText, overlay, theme, docOrigin);
                    }

                    if (opts.codeIndentGuides && vline.isFirstSlice && !vline.sliceStr.empty() && bl.spaceW > 0.0f) {
                        keel::usize leadSpaces = 0;
                        while (leadSpaces < vline.sliceStr.size() && vline.sliceStr[leadSpaces] == ' ') ++leadSpaces;
                        for (keel::usize col = 4; col <= leadSpaces; col += 4) {
                            const keel::f32 gx = keel::round(screenCodeStartX + keel::as<keel::f32>(col) * bl.spaceW);
                            canvas.DrawLine({gx, screenLy}, {gx, screenLy + bl.codeLineH}, theme.palette.borderSubtle.WithAlpha(90), 1.0f);
                        }
                    }

                    if (opts.codeShowWhitespace && !vline.sliceStr.empty() && bl.spaceW > 0.0f) {
                        const keel::f32 dotR = keel::max(1.2f, 1.4f * s);
                        const keel::f32 cy = screenLy + bl.codeLineH * 0.5f;
                        for (keel::usize c = 0; c < vline.sliceStr.size(); ++c) {
                            if (vline.sliceStr[c] == ' ') {
                                const keel::f32 cx = screenCodeStartX + (keel::as<keel::f32>(c) + 0.5f) * bl.spaceW;
                                canvas.DrawRoundRect(Rect(cx - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f), dotR, wsColor);
                            } else if (vline.sliceStr[c] == '\t') {
                                const keel::f32 x0 = screenCodeStartX + keel::as<keel::f32>(c) * bl.spaceW + 2.0f * s;
                                const keel::f32 x1 = screenCodeStartX + keel::as<keel::f32>(c + 1) * bl.spaceW - 2.0f * s;
                                if (x1 > x0) {
                                    canvas.DrawLine({x0, cy}, {x1, cy}, wsColor, 1.2f);
                                    canvas.DrawLine({x1 - 3.0f * s, cy - 2.5f * s}, {x1, cy}, wsColor, 1.2f);
                                    canvas.DrawLine({x1 - 3.0f * s, cy + 2.5f * s}, {x1, cy}, wsColor, 1.2f);
                                }
                            }
                        }
                    }

                    for (const CodeRenderSpan& sp : vline.renderSpans) {
                        canvas.DrawTextStyled(
                            sp.text,
                            {screenCodeStartX + sp.localOffsetX, screenTextY},
                            bl.codeFs,
                            sp.color,
                            FontStyle::Monospace
                        );
                    }

                    if (tok) {
                        DrawTokenCaretIfPresent(canvas, *tok, layout.textMap.plainText, overlay, theme, docOrigin);
                    }
                }
                canvas.PopClip();
                break;
            }

            case BlockKind::ImageBlock: {
                const Rect imgRect = OffsetRect(bl.imgLocalRect, docOrigin);
                if (opts.imageCheckerboard) {
                    detail::Image::DrawCheckerboard(canvas, imgRect, bl.imgRadius, theme, 10.0f);
                } else {
                    canvas.DrawRoundRect(imgRect, bl.imgRadius, theme.palette.surfaceElevated);
                }
                if (!canvas.DrawImage(blk.rawText, imgRect, bl.imgRadius, 255)) {
                    canvas.DrawTextBox(
                        blk.imageAlt.empty() ? blk.rawText.as_ref() : blk.imageAlt.as_ref(),
                        imgRect.Inset(8.0f),
                        theme.GetFontSize(TextRole::LabelMedium) * effZoom,
                        theme.palette.textMuted,
                        0.5f,
                        0.5f
                    );
                }
                canvas.DrawRoundRect(imgRect, bl.imgRadius, Color::Transparent(), theme.palette.borderSubtle, 1.0f);

                if (!bl.flow.items.empty()) {
                    RenderInlineFlowLayout(
                        canvas,
                        bl.flow,
                        docOrigin + bl.flowLocalOrigin,
                        docOrigin,
                        theme.palette.textMuted,
                        &layout.textMap,
                        overlay,
                        theme,
                        localMouse
                    );
                }
                break;
            }

            case BlockKind::Table: {
                if (bl.rowHeights.empty() || bl.colWidths.empty()) break;
                canvas.DrawRoundRect(screenBlk, bl.boxRadius, theme.palette.surface, theme.palette.borderSubtle, 1.0f);

                const keel::usize numRows = bl.rowHeights.size();
                const keel::usize numCols = bl.colWidths.size();
                keel::f32 ry = screenBlk.y;

                for (keel::usize r = 0; r < numRows; ++r) {
                    const keel::f32 rowH = bl.rowHeights[r];
                    const Rect rowRect(screenBlk.x, ry, screenBlk.w, rowH);
                    if (r == 0) {
                        canvas.DrawRoundRect(rowRect, bl.boxRadius, theme.palette.surfaceElevated);
                    } else if (r % 2 == 1) {
                        canvas.DrawRect(rowRect.Inset(1.0f, 0.0f), theme.palette.surfaceElevated.WithAlpha(90));
                    }
                    if (r > 0) {
                        canvas.DrawLine({rowRect.x, ry}, {rowRect.Right(), ry}, theme.palette.borderSubtle, 1.0f);
                    }

                    for (keel::usize c = 0; c < numCols; ++c) {
                        const keel::f32 cx = docOrigin.x + bl.colX[c];
                        const keel::f32 cw = bl.colWidths[c];
                        if (c > 0) {
                            canvas.DrawLine({cx, ry}, {cx, ry + rowH}, theme.palette.borderSubtle, 1.0f);
                        }
                        if (r < bl.cellFlows.size() && c < bl.cellFlows[r].size() && !bl.cellFlows[r][c].items.empty()) {
                            canvas.PushClip(Rect(cx + 1.0f, ry + 1.0f, keel::max(1.0f, cw - 2.0f), keel::max(1.0f, rowH - 2.0f)));
                            RenderInlineFlowLayout(
                                canvas,
                                bl.cellFlows[r][c],
                                docOrigin + bl.cellFlowOrigins[r][c],
                                docOrigin,
                                theme.palette.textPrimary,
                                &layout.textMap,
                                overlay,
                                theme,
                                localMouse
                            );
                            canvas.PopClip();
                        }
                    }
                    ry += rowH;
                }
                break;
            }
        }
    }
}

struct MarkdownState {
    keel::u64 docSourceHash{0};
    keel::i32 copiedBlockIndex{-1};
    keel::f32 zoom{1.0f};
    keel::Arc<const MarkdownDocumentLayout> cachedLayout{};

    // Selection & Cursor state
    keel::usize cursorOffset{0};
    keel::usize selAnchorOffset{0};
    bool isDraggingSelection{false};
    keel::f32 preferredCursorX{-1.0f};
    keel::String cachedSelectedText{};

    // Interactive Task List overrides
    keel::HashMap<keel::i32, bool> taskOverrides{};

    // In-document Search state
    bool searchBarOpen{false};
    keel::String searchQuery{};
    keel::i32 activeSearchMatch{0};
    keel::u64 lastRunSeq{0};

    void TrimInactiveCaches() noexcept {
        cachedLayout.reset();
        cachedSelectedText = {};
        if (lastRunSeq == g_lastAnyMarkdownRunSeq) {
            ClearMarkdownThreadCaches();
        }
    }

    bool HasSelection() const noexcept {
        return cursorOffset != selAnchorOffset;
    }
    keel::usize SelMin() const noexcept {
        return keel::min(cursorOffset, selAnchorOffset);
    }
    keel::usize SelMax() const noexcept {
        return keel::max(cursorOffset, selAnchorOffset);
    }
};

MarkdownOptions CloneMarkdownOptionsWithoutCallbacks(const MarkdownOptions& src) noexcept {
    MarkdownOptions dst{};
    dst.align = src.align;
    dst.width = src.width;
    dst.height = src.height;
    dst.background = src.background;
    dst.border = src.border;
    dst.allowZoom = src.allowZoom;
    dst.zoom = src.zoom;
    dst.bodyFontSize = src.bodyFontSize;
    dst.headingScale = src.headingScale;
    dst.inlineCodeFontSize = src.inlineCodeFontSize;
    dst.codeFontSize = src.codeFontSize;
    dst.lineHeightScale = src.lineHeightScale;
    dst.documentPadding = src.documentPadding;
    dst.maxContentWidth = src.maxContentWidth;
    dst.blockGap = src.blockGap;
    dst.paragraphIndent = src.paragraphIndent;
    dst.listIndent = src.listIndent;
    dst.blockPadding = src.blockPadding;
    dst.blockRadius = src.blockRadius;
    dst.headingDividers = src.headingDividers;
    dst.codeLineNumbers = src.codeLineNumbers;
    dst.syntaxHighlight = src.syntaxHighlight;
    dst.showCopyButton = src.showCopyButton;
    dst.codeShowWhitespace = src.codeShowWhitespace;
    dst.codeIndentGuides = src.codeIndentGuides;
    dst.codePalette = src.codePalette;
    dst.imageCheckerboard = src.imageCheckerboard;
    dst.imageMaxHeight = src.imageMaxHeight;
    dst.imageRadius = src.imageRadius;
    dst.enableSelection = src.enableSelection;
    dst.enableSearch = src.enableSearch;
    dst.interactiveTasks = src.interactiveTasks;
    dst.searchQuery = src.searchQuery;
    dst.tooltip = src.tooltip;
    dst.disabled = src.disabled;
    return dst;
}

struct MarkdownPayload {
    keel::StringRef text{};
    MarkdownOptions opts{};
    keel::f32 zoom{1.0f};
    keel::i32 hoveredCopyBlock{-1};
    keel::i32 activeCopiedBlock{-1};
    keel::i32 hoveredTaskIndex{-1};
    bool hasSelection{false};
    keel::usize selMin{0};
    keel::usize selMax{0};
    bool showCaret{false};
    keel::usize caretOffset{0};
    bool searchBarOpen{false};
    keel::StringRef activeSearchQuery{};
    keel::i32 activeSearchMatch{-1};
    keel::i32 searchMatchCount{0};
    const keel::HashMap<keel::i32, bool>* taskOverrides{nullptr};
    const MarkdownDocumentLayout* layout{nullptr};
    mutable ICanvas* lastCanvas{nullptr};
};

} // namespace

MarkdownResponse detail::Markdown::Run(
    Context& ctx,
    keel::StringRef markdownText,
    const MarkdownOptions& opts,
    keel::SourceLocation loc
) {

    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiSize activeSize = opts.size;
    const UiTheme& theme = ctx.GetTheme();

    MarkdownOptions optsCopy = CloneMarkdownOptionsWithoutCallbacks(opts);
    if (optsCopy.width.IsAuto()) {
        optsCopy.width = Control::ResolveWidthMode(ctx, opts.width, SizeMode::Stretch);
    }
    optsCopy.searchQuery = ctx.AllocString(opts.searchQuery);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);

    auto& state = ctx.GetOrCreatePersistentState<MarkdownState>(id);
    state.lastRunSeq = ++g_lastAnyMarkdownRunSeq;
    const auto doc = GetOrParseMarkdown(markdownText);
    if (state.docSourceHash != doc->sourceHash) {
        state.docSourceHash = doc->sourceHash;
        state.cursorOffset = 0;
        state.selAnchorOffset = 0;
        state.preferredCursorX = -1.0f;
        state.taskOverrides.clear();
    }

    if (optsCopy.zoom != nullptr) {
        state.zoom = keel::clamp(*optsCopy.zoom, 0.5f, 3.0f);
    }

    Response baseRes = ctx.Interact(id);
    MarkdownResponse res{};
    static_cast<Response&>(res) = baseRes;

    const bool isFocused = ctx.IsFocused(id);

    // 1. Handle Smooth Zoom (Ctrl+Wheel & KeyAction::ZoomIn/ZoomOut/ZoomReset)
    if (optsCopy.allowZoom && !effectiveDisabled) {
        bool zoomChanged = false;
        if (baseRes.hovered && ctx.GetKeyModifiers().ctrl && keel::abs(ctx.GetMouseWheel().y) > 0.01f) {
            const keel::f32 step = (ctx.GetMouseWheel().y > 0.0f) ? 0.1f : -0.1f;
            const keel::f32 nextZoom = keel::clamp(keel::round((state.zoom + step) * 10.0f) / 10.0f, 0.5f, 3.0f);
            if (keel::abs(nextZoom - state.zoom) > 0.001f) {
                state.zoom = nextZoom;
                zoomChanged = true;
            }
        }
        if (isFocused || baseRes.hovered) {
            if (ctx.ConsumeAction(KeyAction::ZoomIn)) {
                state.zoom = keel::clamp(keel::round((state.zoom + 0.1f) * 10.0f) / 10.0f, 0.5f, 3.0f);
                zoomChanged = true;
            } else if (ctx.ConsumeAction(KeyAction::ZoomOut)) {
                state.zoom = keel::clamp(keel::round((state.zoom - 0.1f) * 10.0f) / 10.0f, 0.5f, 3.0f);
                zoomChanged = true;
            } else if (ctx.ConsumeAction(KeyAction::ZoomReset)) {
                state.zoom = 1.0f;
                zoomChanged = true;
            }
        }
        if (zoomChanged) {
            if (optsCopy.zoom != nullptr) {
                *optsCopy.zoom = state.zoom;
            }
            res.changed = true;
            ctx.RequestRepaint(2);
        }
    }
    res.currentZoom = state.zoom;

    // 2. Resolve cached Document Layout & DocumentTextMap on previous unclipped bounds (O(1) on cache hit)
    ICanvas* hitCanvas = ctx.GetCanvas();
    Rect hitBounds = ctx.GetPrevUnclippedBounds(id);
    if (hitBounds.w <= 0.0f) {
        hitBounds = Rect(0.0f, 0.0f, 520.0f, 400.0f);
    }
    const Rect ctrlRect = hitBounds;

    state.cachedLayout = GetOrBuildMarkdownLayout(hitCanvas, ctrlRect.w, doc, optsCopy, theme, state.zoom);
    const MarkdownDocumentLayout& layout = *state.cachedLayout;
    const DocumentTextMap& textMap = layout.textMap;

    const MarkdownPassResult hit = HitTestMarkdownLayout(
        layout,
        ctrlRect,
        baseRes.hovered ? ctx.GetMousePos() : Vec2{-100000.0f, -100000.0f},
        state.copiedBlockIndex,
        optsCopy.interactiveTasks,
        optsCopy.showCopyButton,
        effectiveDisabled,
        &state.taskOverrides
    );

    const keel::usize maxOffset = textMap.plainText.size();
    state.cursorOffset = keel::min(state.cursorOffset, maxOffset);
    state.selAnchorOffset = keel::min(state.selAnchorOffset, maxOffset);

    keel::i32 hoveredCopyBlock = hit.hoveredCopyBlock;

    // 3. Handle Links, Copy Button, Task Checkboxes, and Mouse Text Selection
    if (!effectiveDisabled) {
        if (baseRes.hovered) {
            if (!hit.hoveredLink.empty()) {
                ctx.SetTooltip(hit.hoveredLink, hit.hoveredLinkRect);
                if (baseRes.clicked) {
                    res.clickedLink = hit.hoveredLink;
                    res.changed = true;
                    if (opts.onLinkClick) {
                        opts.onLinkClick(hit.hoveredLink);
                    }
                }
            } else if (hit.hoveredCopyBlock >= 0) {
                if (baseRes.clicked) {
                    state.copiedBlockIndex = hit.hoveredCopyBlock;
                    res.copiedBlockIndex = hit.hoveredCopyBlock;
                    res.changed = true;
                    ctx.SetClipboardText(hit.hoveredCopyCode);
                }
            } else if (hit.hoveredTaskIndex >= 0 && optsCopy.interactiveTasks) {
                if (baseRes.clicked) {
                    const bool nextChecked = !hit.hoveredTaskCurrentState;
                    state.taskOverrides.put(hit.hoveredTaskIndex, nextChecked);
                    res.toggledTaskIndex = hit.hoveredTaskIndex;
                    res.toggledTaskChecked = nextChecked;
                    res.changed = true;
                    if (opts.onTaskToggle) {
                        opts.onTaskToggle(hit.hoveredTaskIndex, nextChecked);
                    }
                }
            } else if (!opts.tooltip.empty()) {
                ctx.SetTooltip(opts.tooltip, baseRes.rect);
            }
        }

        // Mouse click / double-click / drag selection (using document-local mouse coordinates)
        const bool clickedInteractiveSubElement =
            !hit.hoveredLink.empty() || hit.hoveredCopyBlock >= 0 || (hit.hoveredTaskIndex >= 0 && optsCopy.interactiveTasks);

        if (optsCopy.enableSelection && baseRes.hovered && ctx.IsMouseClicked() && !clickedInteractiveSubElement) {
            ctx.RequestFocus(id);
            const Vec2 localMouse = ctx.GetMousePos() - ctrlRect.Pos();
            const keel::usize hitOff = HitTestDocumentCharOffset(textMap, hitCanvas, localMouse);
            state.preferredCursorX = -1.0f;
            if (baseRes.doubleClicked) {
                TextUtil::SelectWordAt(textMap.plainText, hitOff, state.selAnchorOffset, state.cursorOffset);
                state.isDraggingSelection = false;
            } else {
                state.cursorOffset = hitOff;
                if (!ctx.GetKeyModifiers().shift) {
                    state.selAnchorOffset = hitOff;
                }
                state.isDraggingSelection = true;
            }
        } else if (state.isDraggingSelection) {
            if (ctx.IsMouseDown()) {
                const Vec2 localMouse = ctx.GetMousePos() - ctrlRect.Pos();
                state.cursorOffset = HitTestDocumentCharOffset(textMap, hitCanvas, localMouse);
                state.preferredCursorX = -1.0f;
            } else {
                state.isDraggingSelection = false;
            }
        }
    }

    // 4. Handle Keyboard Actions via centralized KeyAction system (Search, Copy, SelectAll, Cursor & Selection Navigation)
    if ((isFocused || baseRes.hovered) && !effectiveDisabled) {
        if (optsCopy.enableSearch && ctx.ConsumeAction(KeyAction::Find)) {
            state.searchBarOpen = true;
            ctx.RequestFocus(id);
            ctx.RequestRepaint(2);
        }
    }

    if (isFocused && !effectiveDisabled) {
        if (state.searchBarOpen && optsCopy.enableSearch) {
            if (ctx.ConsumeAction(KeyAction::Cancel)) {
                state.searchBarOpen = false;
                ctx.RequestRepaint(2);
            } else if (ctx.ConsumeAction(KeyAction::FindNext) || ctx.ConsumeAction(KeyAction::Confirm)) {
                state.activeSearchMatch += 1;
            } else if (ctx.ConsumeAction(KeyAction::FindPrev)) {
                state.activeSearchMatch -= 1;
            } else if (ctx.ConsumeAction(KeyAction::DeletePrevChar)) {
                if (!state.searchQuery.empty()) {
                    const keel::usize prev = TextUtil::PrevCharBoundary(state.searchQuery, state.searchQuery.size());
                    state.searchQuery.resize(prev);
                    state.activeSearchMatch = 0;
                }
            } else if (ctx.ConsumeAction(KeyAction::DeletePrevWord)) {
                if (!state.searchQuery.empty()) {
                    const keel::usize prev = TextUtil::PrevWordBoundary(state.searchQuery, state.searchQuery.size());
                    state.searchQuery.resize(prev);
                    state.activeSearchMatch = 0;
                }
            } else {
                const auto typedChunks = ctx.ConsumePendingTextInput();
                if (!typedChunks.empty()) {
                    for (const keel::String& chunk : typedChunks) {
                        for (char ch : chunk) {
                            if (keel::as<keel::u8>(ch) >= 32) {
                                state.searchQuery.push_back(ch);
                            }
                        }
                    }
                    state.activeSearchMatch = 0;
                }
            }
        }

        if (optsCopy.enableSelection) {
            auto moveCursorHoriz = [&](keel::usize newOff, bool extendSelection) {
                state.cursorOffset = keel::min(newOff, maxOffset);
                if (!extendSelection) {
                    state.selAnchorOffset = state.cursorOffset;
                }
                state.preferredCursorX = -1.0f;
                ctx.RequestRepaint(2);
            };

            auto moveCursorVert = [&](keel::i32 lineDelta, bool extendSelection) {
                if (textMap.lines.empty()) return;
                const keel::usize curLine = FindLineForCharOffset(textMap, state.cursorOffset);
                if (state.preferredCursorX < 0.0f) {
                    state.preferredCursorX = GetCursorXForOffset(textMap, hitCanvas, state.cursorOffset);
                }
                const keel::i32 targetLine = keel::clamp(
                    keel::as<keel::i32>(curLine) + lineDelta,
                    0,
                    keel::as<keel::i32>(textMap.lines.size()) - 1
                );
                const PositionedLine& ln = textMap.lines[keel::as<keel::usize>(targetLine)];
                const keel::f32 targetY = (ln.top + ln.bottom) * 0.5f;
                state.cursorOffset = HitTestDocumentCharOffset(textMap, hitCanvas, {state.preferredCursorX, targetY});
                if (!extendSelection) {
                    state.selAnchorOffset = state.cursorOffset;
                }
                ctx.RequestRepaint(2);
            };

            if (ctx.ConsumeAction(KeyAction::Copy)) {
                if (state.HasSelection()) {
                    const keel::usize s0 = state.SelMin();
                    const keel::usize s1 = state.SelMax();
                    ctx.SetClipboardText(textMap.plainText.as_ref().substr(s0, s1 - s0));
                }
            } else if (ctx.ConsumeAction(KeyAction::SelectAll)) {
                state.selAnchorOffset = 0;
                state.cursorOffset = maxOffset;
                state.preferredCursorX = -1.0f;
            } else if (ctx.ConsumeAction(KeyAction::Cancel)) {
                state.selAnchorOffset = state.cursorOffset;
            } else if (ctx.ConsumeAction(KeyAction::MoveLeft)) {
                if (state.HasSelection()) {
                    moveCursorHoriz(state.SelMin(), false);
                } else {
                    moveCursorHoriz(TextUtil::PrevCharBoundary(textMap.plainText, state.cursorOffset), false);
                }
            } else if (ctx.ConsumeAction(KeyAction::MoveRight)) {
                if (state.HasSelection()) {
                    moveCursorHoriz(state.SelMax(), false);
                } else {
                    moveCursorHoriz(TextUtil::NextCharBoundary(textMap.plainText, state.cursorOffset), false);
                }
            } else if (ctx.ConsumeAction(KeyAction::SelectLeft)) {
                moveCursorHoriz(TextUtil::PrevCharBoundary(textMap.plainText, state.cursorOffset), true);
            } else if (ctx.ConsumeAction(KeyAction::SelectRight)) {
                moveCursorHoriz(TextUtil::NextCharBoundary(textMap.plainText, state.cursorOffset), true);
            } else if (ctx.ConsumeAction(KeyAction::MoveWordLeft)) {
                moveCursorHoriz(TextUtil::PrevWordBoundary(textMap.plainText, state.cursorOffset), false);
            } else if (ctx.ConsumeAction(KeyAction::MoveWordRight)) {
                moveCursorHoriz(TextUtil::NextWordBoundary(textMap.plainText, state.cursorOffset), false);
            } else if (ctx.ConsumeAction(KeyAction::SelectWordLeft)) {
                moveCursorHoriz(TextUtil::PrevWordBoundary(textMap.plainText, state.cursorOffset), true);
            } else if (ctx.ConsumeAction(KeyAction::SelectWordRight)) {
                moveCursorHoriz(TextUtil::NextWordBoundary(textMap.plainText, state.cursorOffset), true);
            } else if (ctx.ConsumeAction(KeyAction::MoveUp)) {
                moveCursorVert(-1, false);
            } else if (ctx.ConsumeAction(KeyAction::MoveDown)) {
                moveCursorVert(1, false);
            } else if (ctx.ConsumeAction(KeyAction::SelectUp)) {
                moveCursorVert(-1, true);
            } else if (ctx.ConsumeAction(KeyAction::SelectDown)) {
                moveCursorVert(1, true);
            } else if (ctx.ConsumeAction(KeyAction::MovePageUp)) {
                moveCursorVert(-10, false);
            } else if (ctx.ConsumeAction(KeyAction::MovePageDown)) {
                moveCursorVert(10, false);
            } else if (ctx.ConsumeAction(KeyAction::SelectPageUp)) {
                moveCursorVert(-10, true);
            } else if (ctx.ConsumeAction(KeyAction::SelectPageDown)) {
                moveCursorVert(10, true);
            } else if (ctx.ConsumeAction(KeyAction::MoveLineStart)) {
                if (!textMap.lines.empty()) {
                    const keel::usize li = FindLineForCharOffset(textMap, state.cursorOffset);
                    moveCursorHoriz(textMap.lines[li].charStart, false);
                }
            } else if (ctx.ConsumeAction(KeyAction::MoveLineEnd)) {
                if (!textMap.lines.empty()) {
                    const keel::usize li = FindLineForCharOffset(textMap, state.cursorOffset);
                    moveCursorHoriz(textMap.lines[li].charEnd, false);
                }
            } else if (ctx.ConsumeAction(KeyAction::SelectLineStart)) {
                if (!textMap.lines.empty()) {
                    const keel::usize li = FindLineForCharOffset(textMap, state.cursorOffset);
                    moveCursorHoriz(textMap.lines[li].charStart, true);
                }
            } else if (ctx.ConsumeAction(KeyAction::SelectLineEnd)) {
                if (!textMap.lines.empty()) {
                    const keel::usize li = FindLineForCharOffset(textMap, state.cursorOffset);
                    moveCursorHoriz(textMap.lines[li].charEnd, true);
                }
            } else if (ctx.ConsumeAction(KeyAction::MoveDocStart)) {
                moveCursorHoriz(0, false);
            } else if (ctx.ConsumeAction(KeyAction::MoveDocEnd)) {
                moveCursorHoriz(maxOffset, false);
            } else if (ctx.ConsumeAction(KeyAction::SelectDocStart)) {
                moveCursorHoriz(0, true);
            } else if (ctx.ConsumeAction(KeyAction::SelectDocEnd)) {
                moveCursorHoriz(maxOffset, true);
            }
        }
    }

    // 5. Compute Search Matches & Selected Text for Response
    const keel::StringRef effectiveQuery = !optsCopy.searchQuery.empty()
        ? optsCopy.searchQuery
        : state.searchQuery.as_ref();
    const auto matches = FindSearchMatches(textMap.plainText, effectiveQuery);
    res.searchMatchCount = keel::as<keel::i32>(matches.size());
    if (!matches.empty()) {
        if (state.activeSearchMatch < 0) {
            state.activeSearchMatch = keel::as<keel::i32>(matches.size()) - 1;
        } else {
            state.activeSearchMatch = state.activeSearchMatch % keel::as<keel::i32>(matches.size());
        }
        res.activeSearchMatch = state.activeSearchMatch;
    } else {
        res.activeSearchMatch = -1;
    }

    if (state.HasSelection() && state.SelMax() <= textMap.plainText.size()) {
        state.cachedSelectedText.assign(textMap.plainText.as_ref().substr(state.SelMin(), state.SelMax() - state.SelMin()));
        res.selectedText = state.cachedSelectedText.as_ref();
    }

    // 6. Create Node & Payload
    Node* node = ctx.CreateNode(NodeType::Markdown, id);
    auto* payload = ctx.AllocPayload<MarkdownPayload>();
    payload->text = ctx.AllocString(markdownText);
    payload->opts = CloneMarkdownOptionsWithoutCallbacks(optsCopy);
    payload->zoom = state.zoom;
    payload->hoveredCopyBlock = hoveredCopyBlock;
    payload->activeCopiedBlock = state.copiedBlockIndex;
    payload->hoveredTaskIndex = hit.hoveredTaskIndex;
    payload->hasSelection = optsCopy.enableSelection && state.HasSelection();
    payload->selMin = state.SelMin();
    payload->selMax = state.SelMax();
    payload->showCaret = optsCopy.enableSelection && isFocused;
    payload->caretOffset = state.cursorOffset;
    payload->searchBarOpen = state.searchBarOpen && optsCopy.enableSearch;
    payload->activeSearchQuery = ctx.AllocString(effectiveQuery);
    payload->activeSearchMatch = res.activeSearchMatch;
    payload->searchMatchCount = res.searchMatchCount;
    payload->taskOverrides = &state.taskOverrides;
    payload->layout = state.cachedLayout.get();
    payload->lastCanvas = ctx.GetCanvas();
    node->SetPayload(payload);

    node->visualHash = detail::HashArgs(
        doc->sourceHash,
        state.zoom,
        keel::as<keel::u8>(optsCopy.align),
        optsCopy.width,
        optsCopy.height,
        optsCopy.background,
        optsCopy.border,
        optsCopy.bodyFontSize.value_or(-1.0f),
        optsCopy.headingScale,
        optsCopy.inlineCodeFontSize.value_or(-1.0f),
        optsCopy.codeFontSize.value_or(-1.0f),
        optsCopy.lineHeightScale,
        optsCopy.documentPadding,
        optsCopy.maxContentWidth.value_or(-1.0f),
        optsCopy.blockGap,
        optsCopy.paragraphIndent,
        optsCopy.listIndent,
        optsCopy.blockPadding,
        optsCopy.blockRadius,
        optsCopy.headingDividers,
        optsCopy.codeLineNumbers,
        optsCopy.syntaxHighlight,
        optsCopy.showCopyButton,
        optsCopy.codeShowWhitespace,
        optsCopy.codeIndentGuides,
        optsCopy.codePalette.has_value() ? optsCopy.codePalette->editorBg.ToUint32() : 0u,
        optsCopy.codePalette.has_value() ? optsCopy.codePalette->keyword.ToUint32() : 0u,
        optsCopy.imageCheckerboard,
        optsCopy.imageMaxHeight,
        optsCopy.imageRadius,
        hoveredCopyBlock,
        hit.hoveredTaskIndex,
        state.copiedBlockIndex,
        payload->hasSelection,
        payload->selMin,
        payload->selMax,
        payload->showCaret,
        payload->caretOffset,
        payload->searchBarOpen,
        payload->activeSearchQuery,
        payload->activeSearchMatch,
        state.taskOverrides.size(),
        activeSize,
        effectiveDisabled
    );

    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& th) noexcept -> Vec2 {
        const auto* p = n->As<MarkdownPayload>();
        if (!p) return {0.0f, 0.0f};
        if (canvas) {
            p->lastCanvas = canvas;
        }
        if (availableWidth <= 0.0f && p->layout != nullptr) {
            const keel::f32 finalH = p->opts.height.IsFixed()
                ? p->opts.height.Dp()
                : p->layout->totalHeight;
            return {keel::max(p->layout->outerWidth, p->layout->totalWidth), finalH};
        }
        MarkdownOptions mOpts = CloneMarkdownOptionsWithoutCallbacks(p->opts);
        if (n->widthMode == SizeMode::Stretch && !mOpts.width.IsFixed()) {
            mOpts.width = SizeMode::Stretch;
        }
        keel::f32 z = p->zoom;
        mOpts.zoom = &z;
        return detail::Markdown::Measure(p->text, mOpts, n->uiSize, canvas ? canvas : p->lastCanvas, th, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& th) noexcept {
        const auto* p = n->As<MarkdownPayload>();
        if (!p) return;

        if (p->opts.background.IsBackgroundEnabled(false) || p->opts.border.color.HasAny() || p->opts.border.width > 0.0f) {
            const bool interactiveBg = !p->opts.background.IsSurface();
            const Color bgCol = p->opts.background.IsBackgroundEnabled(false)
                ? th.ResolveColor(p->opts.background, interactiveBg && n->hovered, interactiveBg && n->pressed, n->disabled, th.palette.surface)
                : Color::Transparent();
            const Color bCol = th.ResolveColor(p->opts.border.color, n->hovered, n->pressed, n->disabled, th.palette.border);
            const keel::f32 bW = (p->opts.border.width > 0.0f) ? p->opts.border.width : (p->opts.border.color.HasAny() ? 1.0f : 0.0f);
            const keel::f32 bR = p->opts.border.radius.has_value() ? *p->opts.border.radius : 0.0f;
            canvas.DrawRoundRect(n->computedRect, bR, bgCol, bCol, bW);
        }

        const Rect ctrlRect = n->computedRect;

        // Compute visible viewport rect in the Markdown node's local space by intersecting with clipping ancestors,
        // transforming across any scaled Zone boundaries so O(1) block frustum culling never clips visible blocks.
        Rect visibleRect = ctrlRect;
        Transform2D nodeToAnc{};
        for (const Node* cur = n; cur->parent != nullptr; cur = cur->parent) {
            const keel::f32 relScale = cur->GetRelativeScale();
            if (relScale > 0.0f && keel::abs(relScale - 1.0f) > 1e-6f) {
                const Transform2D localToParent = Transform2D::ScaleAbout(
                    Vec2{cur->computedRect.x, cur->computedRect.y},
                    relScale
                );
                nodeToAnc = localToParent.Then(nodeToAnc);
            }
            const Node* anc = cur->parent;
            if (anc->clipChildren) {
                const Rect ancLocalRect = nodeToAnc.IsIdentity()
                    ? anc->computedRect
                    : nodeToAnc.Invert(anc->computedRect);
                visibleRect = visibleRect.Intersect(ancLocalRect);
            }
        }

        keel::Arc<const MarkdownDocumentLayout> layoutHolder;
        const MarkdownDocumentLayout& layout =
            (p->layout != nullptr && keel::abs(p->layout->outerWidth - ctrlRect.w) < 1.0f)
                ? *p->layout
                : *(layoutHolder = GetOrBuildMarkdownLayout(&canvas, ctrlRect.w, GetOrParseMarkdown(p->text), p->opts, th, p->zoom));
        const ParsedMarkdownDoc& parsedDoc = *layout.doc;

        RenderOverlayState overlay{};
        overlay.hasSelection = p->hasSelection;
        overlay.selMin = p->selMin;
        overlay.selMax = p->selMax;
        overlay.showCaret = p->showCaret;
        overlay.caretOffset = p->caretOffset;
        overlay.activeSearchMatch = p->activeSearchMatch;
        overlay.taskOverrides = p->taskOverrides;

        keel::Array<SearchMatchRange> matches;
        if (!p->activeSearchQuery.empty()) {
            matches = FindSearchMatches(layout.textMap.plainText, p->activeSearchQuery);
            overlay.searchMatches = &matches;
        }

        RenderMarkdownDocumentLayout(
            canvas,
            parsedDoc,
            layout,
            ctrlRect,
            visibleRect,
            p->opts,
            th,
            {-100000.0f, -100000.0f},
            p->hoveredCopyBlock,
            p->activeCopiedBlock,
            p->hoveredTaskIndex,
            &overlay
        );

        // Render floating in-document Search bar if open (pinned to visible viewport top-right)
        if (p->searchBarOpen) {
            constexpr keel::f32 s = 1.0f;
            const keel::f32 barW = 240.0f * s;
            const keel::f32 barH = 30.0f * s;
            const keel::f32 anchorTop = (visibleRect.h > 0.0f) ? visibleRect.y : ctrlRect.y;
            const keel::f32 anchorRight = (visibleRect.w > 0.0f) ? visibleRect.Right() : ctrlRect.Right();
            const Rect barRect(
                anchorRight - barW - 10.0f * s,
                anchorTop + 8.0f * s,
                barW,
                barH
            );
            canvas.DrawRoundRect(barRect, 6.0f * s, th.palette.surfaceElevated, th.palette.accent, 1.2f);
            const keel::f32 fs = th.GetFontSize(TextRole::BodySmall);
            const keel::StringRef disp = p->activeSearchQuery.empty()
                ? keel::StringRef("Find in document...")
                : p->activeSearchQuery;
            canvas.DrawTextBox(
                disp,
                Rect(barRect.x + 10.0f * s, barRect.y, barW - 76.0f * s, barH),
                fs,
                p->activeSearchQuery.empty() ? th.palette.textMuted : th.palette.textPrimary,
                0.0f,
                0.5f
            );
            char countBuf[32];
            if (p->searchMatchCount > 0) {
                std::snprintf(countBuf, sizeof(countBuf), "%d/%d", p->activeSearchMatch + 1, p->searchMatchCount);
            } else {
                std::snprintf(countBuf, sizeof(countBuf), "0/0");
            }
            canvas.DrawTextBox(
                countBuf,
                Rect(barRect.Right() - 64.0f * s, barRect.y, 56.0f * s, barH),
                th.GetFontSize(TextRole::LabelSmall),
                th.palette.textMuted,
                1.0f,
                0.5f
            );
        }
    };

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = baseRes.hovered;
    node->pressed = baseRes.pressed;
    node->focused = isFocused;

    ControlSpec ctrlSpec{};
    ctrlSpec.width = optsCopy.width;
    ctrlSpec.height = optsCopy.height;
    ctrlSpec.defaultWidthMode = SizeMode::Stretch;
    ctrlSpec.defaultHeightMode = SizeMode::Fit;
    Control::SetupNode(ctx, node, ctrlSpec);
    if ((!node->parent || node->parent->type != NodeType::Element) &&
        optsCopy.height.ResolveMode(SizeMode::Fit) == SizeMode::Fit &&
        node->heightMode == SizeMode::Stretch) {
        node->heightMode = SizeMode::Fit;
    }

    return res;
}

Vec2 detail::Markdown::Measure(
    keel::StringRef markdownText,
    const MarkdownOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 availableWidth
) {
    (void)size;
    const auto doc = GetOrParseMarkdown(markdownText);
    const keel::f32 effZoom = (opts.zoom != nullptr) ? keel::clamp(*opts.zoom, 0.5f, 3.0f) : 1.0f;

    keel::f32 ctrlW = (availableWidth > 0.0f) ? availableWidth : 520.0f;
    if (opts.width.IsFixed()) {
        ctrlW = opts.width.Dp();
    } else if (opts.width.ResolveMode(SizeMode::Stretch) == SizeMode::Stretch && availableWidth > 0.0f) {
        ctrlW = availableWidth;
    }

    const auto layout = GetOrBuildMarkdownLayout(canvas, ctrlW, doc, opts, theme, effZoom);

    const keel::f32 finalH = opts.height.IsFixed()
        ? opts.height.Dp()
        : layout->totalHeight;
    return { keel::max(ctrlW, layout->totalWidth), finalH };
}

void detail::Markdown::Render(
    ICanvas& canvas,
    Rect bounds,
    keel::StringRef markdownText,
    const MarkdownOptions& opts,
    UiSize size,
    Vec2 mousePos,
    keel::i32 hoveredCopyBlock,
    keel::i32 activeCopiedBlock,
    bool disabled,
    const UiTheme& theme
) {
    (void)size;
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    if (opts.background.IsBackgroundEnabled(false) || opts.border.color.HasAny() || opts.border.width > 0.0f) {
        const Color bgCol = opts.background.IsBackgroundEnabled(false)
            ? theme.ResolveColor(opts.background, false, false, disabled, theme.palette.surface)
            : Color::Transparent();
        const Color bCol = theme.ResolveColor(opts.border.color, false, false, disabled, theme.palette.border);
        const keel::f32 bW = (opts.border.width > 0.0f) ? opts.border.width : (opts.border.color.HasAny() ? 1.0f : 0.0f);
        const keel::f32 bR = opts.border.radius.has_value() ? *opts.border.radius : 0.0f;
        canvas.DrawRoundRect(bounds, bR, bgCol, bCol, bW);
    }

    const auto doc = GetOrParseMarkdown(markdownText);
    const keel::f32 effZoom = (opts.zoom != nullptr) ? keel::clamp(*opts.zoom, 0.5f, 3.0f) : 1.0f;

    const auto layout = GetOrBuildMarkdownLayout(&canvas, bounds.w, doc, opts, theme, effZoom);
    const MarkdownPassResult hit = HitTestMarkdownLayout(
        *layout,
        bounds,
        mousePos,
        activeCopiedBlock,
        opts.interactiveTasks,
        opts.showCopyButton,
        disabled,
        nullptr
    );

    RenderMarkdownDocumentLayout(
        canvas,
        *doc,
        *layout,
        bounds,
        bounds,
        opts,
        theme,
        mousePos,
        (hoveredCopyBlock >= 0) ? hoveredCopyBlock : hit.hoveredCopyBlock,
        activeCopiedBlock,
        hit.hoveredTaskIndex,
        nullptr
    );
}

} // namespace guikit
