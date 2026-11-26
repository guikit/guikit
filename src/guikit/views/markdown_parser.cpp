#include "markdown_internal.hpp"

namespace guikit::detail::md_impl {

keel::StringRef TrimView(keel::StringRef sv) noexcept {
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r')) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r')) {
        sv.remove_suffix(1);
    }
    return sv;
}

keel::Array<keel::StringRef> SplitTableCells(keel::StringRef rowLine) {
    keel::StringRef s = TrimView(rowLine);
    if (!s.empty() && s.front() == '|') s.remove_prefix(1);
    if (!s.empty() && s.back() == '|') s.remove_suffix(1);

    keel::Array<keel::StringRef> cells;
    keel::usize pos = 0;
    while (pos <= s.size()) {
        const keel::usize pipe = s.find('|', pos);
        if (pipe == keel::StringRef::npos) {
            cells.push_back(TrimView(s.substr(pos)));
            break;
        }
        cells.push_back(TrimView(s.substr(pos, pipe - pos)));
        pos = pipe + 1;
    }
    return cells;
}

bool IsTableSeparatorRow(keel::StringRef line, keel::Array<Align>& outAligns) {
    const keel::StringRef t = TrimView(line);
    if (t.find('|') == keel::StringRef::npos || t.find('-') == keel::StringRef::npos) {
        return false;
    }
    const auto cells = SplitTableCells(t);
    if (cells.empty()) return false;

    keel::Array<Align> aligns;
    aligns.reserve(cells.size());
    for (keel::StringRef c : cells) {
        if (c.empty()) return false;
        bool hasDash = false;
        for (char ch : c) {
            if (ch == '-') hasDash = true;
            else if (ch != ':' && ch != ' ') return false;
        }
        if (!hasDash) return false;
        const bool leftColon = (c.front() == ':');
        const bool rightColon = (c.back() == ':');
        if (leftColon && rightColon) aligns.push_back(Align::Center);
        else if (rightColon) aligns.push_back(Align::End);
        else aligns.push_back(Align::Start);
    }
    outAligns = keel::move(aligns);
    return true;
}


auto& GetMarkdownParseCache() {
    thread_local keel::LruCache<keel::u64, keel::Arc<const ParsedMarkdownDoc>, 8> s_cache;
    return s_cache;
}

keel::Arc<const ParsedMarkdownDoc> GetOrParseMarkdown(keel::StringRef source) {
    auto& cache = GetMarkdownParseCache();
    const keel::u64 h = detail::HashArgs(source, source.size());
    if (const auto* found = cache.find(h)) {
        return *found;
    }

    auto docPtr = keel::make_arc<ParsedMarkdownDoc>();
    ParsedMarkdownDoc& doc = *docPtr;
    doc.sourceHash = h;

    keel::Array<keel::StringRef> lines;
    keel::usize pos = 0;
    while (pos <= source.size()) {
        const keel::usize nl = source.find('\n', pos);
        if (nl == keel::StringRef::npos) {
            lines.push_back(source.substr(pos));
            break;
        }
        keel::StringRef l = source.substr(pos, nl - pos);
        if (!l.empty() && l.back() == '\r') l.remove_suffix(1);
        lines.push_back(l);
        pos = nl + 1;
    }

    keel::i32 nextCodeBlockIdx = 0;
    keel::i32 nextTaskIdx = 0;
    keel::usize i = 0;
    while (i < lines.size()) {
        const keel::StringRef rawLine = lines[i];
        const keel::StringRef trimmed = TrimView(rawLine);

        if (trimmed.empty()) {
            ++i;
            continue;
        }

        // 1. Fenced Code Block: ```lang
        if (trimmed.starts_with("```")) {
            MarkdownBlock blk{};
            blk.kind = BlockKind::CodeBlock;
            blk.codeLang.assign(TrimView(trimmed.substr(3)));
            blk.codeBlockIndex = nextCodeBlockIdx++;
            ++i;
            keel::String code;
            while (i < lines.size()) {
                if (TrimView(lines[i]).starts_with("```")) {
                    ++i;
                    break;
                }
                if (!code.empty()) code.push_back('\n');
                code.append(lines[i]);
                ++i;
            }
            blk.rawText = keel::move(code);
            doc.blocks.push_back(keel::move(blk));
            continue;
        }

        // 2. Horizontal Divider: --- or ***
        if ((trimmed.starts_with("---") || trimmed.starts_with("***")) && trimmed.size() >= 3) {
            bool allSame = true;
            for (char ch : trimmed) {
                if (ch != '-' && ch != '*' && ch != ' ') {
                    allSame = false;
                    break;
                }
            }
            if (allSame) {
                MarkdownBlock blk{};
                blk.kind = BlockKind::Divider;
                doc.blocks.push_back(keel::move(blk));
                ++i;
                continue;
            }
        }

        // 3. Heading: # .. ######
        if (trimmed.front() == '#') {
            keel::u8 lvl = 0;
            while (lvl < trimmed.size() && lvl < 6 && trimmed[lvl] == '#') {
                ++lvl;
            }
            if (lvl < trimmed.size() && trimmed[lvl] == ' ') {
                MarkdownBlock blk{};
                blk.kind = BlockKind::Heading;
                blk.headingLevel = lvl;
                blk.spans = ParseInlineSpans(TrimView(trimmed.substr(lvl + 1)));
                doc.blocks.push_back(keel::move(blk));
                ++i;
                continue;
            }
        }

        // 4. Standalone Image: ![Alt](path)
        if (trimmed.starts_with("![")) {
            const keel::usize closeBr = trimmed.find("](");
            const keel::usize closePar = (closeBr != keel::StringRef::npos) ? trimmed.find(')', closeBr + 2) : keel::StringRef::npos;
            if (closeBr != keel::StringRef::npos && closePar != keel::StringRef::npos) {
                MarkdownBlock blk{};
                blk.kind = BlockKind::ImageBlock;
                blk.imageAlt.assign(trimmed.substr(2, closeBr - 2));
                blk.rawText.assign(TrimView(trimmed.substr(closeBr + 2, closePar - closeBr - 2)));
                if (!blk.imageAlt.empty()) {
                    blk.spans = ParseInlineSpans(blk.imageAlt, FontStyle::Italic, false);
                }
                doc.blocks.push_back(keel::move(blk));
                ++i;
                continue;
            }
        }

        // 5. Blockquote or GitHub Alert: > [!NOTE] / > text
        if (trimmed.starts_with(">")) {
            keel::StringRef firstContent = TrimView(trimmed.substr(1));
            AlertKind alert = AlertKind::None;
            keel::String alertTitle;
            if (firstContent.starts_with("[!NOTE]"))          { alert = AlertKind::Note;      alertTitle.assign("Note"); }
            else if (firstContent.starts_with("[!TIP]"))      { alert = AlertKind::Tip;       alertTitle.assign("Tip"); }
            else if (firstContent.starts_with("[!IMPORTANT]")){ alert = AlertKind::Important; alertTitle.assign("Important"); }
            else if (firstContent.starts_with("[!WARNING]"))  { alert = AlertKind::Warning;   alertTitle.assign("Warning"); }
            else if (firstContent.starts_with("[!CAUTION]"))  { alert = AlertKind::Caution;   alertTitle.assign("Caution"); }

            keel::String combined;
            if (alert != AlertKind::None) {
                const keel::usize closeBr = firstContent.find(']');
                keel::StringRef rest = (closeBr != keel::StringRef::npos) ? TrimView(firstContent.substr(closeBr + 1)) : keel::StringRef{};
                if (!rest.empty()) combined.append(rest);
                ++i;
            }

            while (i < lines.size()) {
                const keel::StringRef ql = TrimView(lines[i]);
                if (!ql.starts_with(">")) break;
                const keel::StringRef sub = TrimView(ql.substr(1));
                if (!sub.empty()) {
                    if (!combined.empty()) combined.push_back(' ');
                    combined.append(sub);
                }
                ++i;
            }

            MarkdownBlock blk{};
            blk.kind = (alert != AlertKind::None) ? BlockKind::Alert : BlockKind::Blockquote;
            blk.alertKind = alert;
            blk.alertTitle = keel::move(alertTitle);
            blk.spans = ParseInlineSpans(combined);
            doc.blocks.push_back(keel::move(blk));
            continue;
        }

        // 6. Table: | Header | ... followed by | --- | --- |
        keel::Array<Align> tableAligns;
        if (trimmed.find('|') != keel::StringRef::npos && i + 1 < lines.size() &&
            IsTableSeparatorRow(lines[i + 1], tableAligns)) {
            MarkdownBlock blk{};
            blk.kind = BlockKind::Table;
            blk.tableAligns = keel::move(tableAligns);

            auto parseRow = [](keel::StringRef rowLine) {
                const auto rawCells = SplitTableCells(rowLine);
                keel::Array<TableCell> row;
                row.reserve(rawCells.size());
                for (keel::StringRef c : rawCells) {
                    row.push_back(TableCell{ParseInlineSpans(c)});
                }
                return row;
            };

            blk.tableRows.push_back(parseRow(lines[i]));
            i += 2; // Skip header and separator row
            while (i < lines.size()) {
                const keel::StringRef tl = TrimView(lines[i]);
                if (tl.empty() || tl.find('|') == keel::StringRef::npos) break;
                blk.tableRows.push_back(parseRow(tl));
                ++i;
            }
            doc.blocks.push_back(keel::move(blk));
            continue;
        }

        // 7. Unordered / Task List Item (- item, * item, - [x] task) or Ordered List Item (1. item)
        keel::usize leadingSpaces = 0;
        while (leadingSpaces < rawLine.size() && rawLine[leadingSpaces] == ' ') {
            ++leadingSpaces;
        }
        const keel::u8 indentLvl = keel::as<keel::u8>(keel::min<keel::usize>(3, leadingSpaces / 2));

        if ((trimmed.starts_with("- ") || trimmed.starts_with("* ")) && trimmed.size() > 2) {
            MarkdownBlock blk{};
            blk.kind = BlockKind::ListItem;
            blk.indentLevel = indentLvl;
            keel::StringRef itemText = TrimView(trimmed.substr(2));
            if (itemText.starts_with("[ ] ")) {
                blk.taskState = 0;
                blk.taskIndex = nextTaskIdx++;
                itemText = TrimView(itemText.substr(4));
            } else if (itemText.starts_with("[x] ") || itemText.starts_with("[X] ")) {
                blk.taskState = 1;
                blk.taskIndex = nextTaskIdx++;
                itemText = TrimView(itemText.substr(4));
            }
            blk.spans = ParseInlineSpans(itemText);
            doc.blocks.push_back(keel::move(blk));
            ++i;
            continue;
        }

        keel::usize digitEnd = 0;
        while (digitEnd < trimmed.size() && trimmed[digitEnd] >= '0' && trimmed[digitEnd] <= '9') {
            ++digitEnd;
        }
        if (digitEnd > 0 && digitEnd + 1 < trimmed.size() && trimmed[digitEnd] == '.' && trimmed[digitEnd + 1] == ' ') {
            MarkdownBlock blk{};
            blk.kind = BlockKind::ListItem;
            blk.indentLevel = indentLvl;
            blk.isOrderedList = true;
            keel::u16 num = 0;
            for (keel::usize d = 0; d < digitEnd; ++d) {
                num = keel::as<keel::u16>(num * 10 + (trimmed[d] - '0'));
            }
            blk.listNumber = keel::max<keel::u16>(1, num);
            blk.spans = ParseInlineSpans(TrimView(trimmed.substr(digitEnd + 2)));
            doc.blocks.push_back(keel::move(blk));
            ++i;
            continue;
        }

        // 8. Standard Paragraph (merge consecutive plain lines)
        keel::String paraText = keel::String::from(trimmed);
        ++i;
        while (i < lines.size()) {
            const keel::StringRef nextT = TrimView(lines[i]);
            if (nextT.empty() || nextT.starts_with("#") || nextT.starts_with("```") ||
                nextT.starts_with(">") || nextT.starts_with("- ") || nextT.starts_with("* ") ||
                nextT.starts_with("![") || nextT.starts_with("---") || nextT.starts_with("|")) {
                break;
            }
            paraText.push_back(' ');
            paraText.append(nextT);
            ++i;
        }

        MarkdownBlock blk{};
        blk.kind = BlockKind::Paragraph;
        blk.spans = ParseInlineSpans(paraText);
        doc.blocks.push_back(keel::move(blk));
    }

    return cache.put(h, keel::move(docPtr));
}


keel::f32 HeadingFontSize(keel::u8 level, const MarkdownOptions& opts, const UiTheme& theme, keel::f32 zoom) noexcept {
    const HeadingLevel hl = keel::from_underlying<HeadingLevel>(keel::clamp<keel::u8>(level, 1, 6) - 1);
    const keel::f32 base = theme.GetHeading(hl).titleFontSize;
    return base * keel::clamp(opts.headingScale, 0.5f, 2.5f) * zoom;
}

Color AlertAccentColor(AlertKind kind, const UiTheme& theme) noexcept {
    switch (kind) {
        case AlertKind::Note:      return theme.GetSemanticAccent(UiVariant::Info);
        case AlertKind::Tip:       return theme.GetSemanticAccent(UiVariant::Success);
        case AlertKind::Important: return theme.GetSemanticAccent(UiVariant::Primary);
        case AlertKind::Warning:   return theme.GetSemanticAccent(UiVariant::Warning);
        case AlertKind::Caution:   return theme.GetSemanticAccent(UiVariant::Danger);
        default:                   return theme.palette.accent;
    }
}

Icon AlertIcon(AlertKind kind) noexcept {
    switch (kind) {
        case AlertKind::Note:      return Icon::Info();
        case AlertKind::Tip:       return Icon::Check();
        case AlertKind::Important: return Icon::Flash();
        case AlertKind::Warning:   return Icon::Warning();
        case AlertKind::Caution:   return Icon::Error();
        default:                   return Icon::Info();
    }
}


keel::u64 ComputeMarkdownLayoutKey(
    keel::u64 docSourceHash,
    keel::f32 outerWidth,
    keel::f32 effZoom,
    const MarkdownOptions& opts,
    const UiTheme& theme,
    bool hasCanvasMetrics
) noexcept {
    const keel::i32 qWidth = keel::round_i32(outerWidth * 2.0f);
    const keel::i32 qZoom = keel::round_i32(effZoom * 1000.0f);
    return detail::HashArgs(
        docSourceHash,
        qWidth,
        qZoom,
        hasCanvasMetrics,
        theme.mode,
        theme.palette.textPrimary.ToUint32(),
        theme.palette.accent.ToUint32(),
        keel::as<keel::u8>(opts.align),
        opts.bodyFontSize.value_or(-1.0f),
        opts.headingScale,
        opts.inlineCodeFontSize.value_or(-1.0f),
        opts.codeFontSize.value_or(-1.0f),
        opts.lineHeightScale,
        opts.documentPadding,
        opts.maxContentWidth.value_or(-1.0f),
        opts.blockGap,
        opts.paragraphIndent,
        opts.listIndent,
        opts.blockPadding,
        opts.blockRadius,
        opts.headingDividers,
        opts.codeLineNumbers,
        opts.syntaxHighlight,
        opts.codePalette.has_value() ? opts.codePalette->editorBg.ToUint32() : 0u,
        opts.codePalette.has_value() ? opts.codePalette->keyword.ToUint32() : 0u,
        opts.imageMaxHeight,
        opts.imageRadius
    );
}

keel::Arc<const MarkdownDocumentLayout> BuildMarkdownDocumentLayout(
    ICanvas* canvas,
    keel::f32 outerWidth,
    const keel::Arc<const ParsedMarkdownDoc>& docPtr,
    const MarkdownOptions& opts,
    const UiTheme& theme,
    keel::f32 zoom,
    keel::u64 layoutKey
) {
    auto layoutPtr = keel::make_arc<MarkdownDocumentLayout>();
    MarkdownDocumentLayout& out = *layoutPtr;
    out.doc = docPtr;
    out.layoutKey = layoutKey;
    out.outerWidth = outerWidth;
    const ParsedMarkdownDoc& doc = *docPtr;

    const keel::f32 effZoom = keel::clamp(zoom, 0.5f, 3.0f);
    const keel::f32 s = effZoom;
    const keel::f32 docPad = keel::max(0.0f, opts.documentPadding) * s;

    out.effZoom = effZoom;
    out.scale = s;

    keel::f32 availW = keel::max(120.0f, outerWidth - docPad * 2.0f);
    keel::f32 startX = docPad;
    if (opts.maxContentWidth.has_value() && *opts.maxContentWidth > 0.0f) {
        const keel::f32 maxColW = *opts.maxContentWidth * s;
        if (availW > maxColW) {
            startX += (availW - maxColW) * 0.5f;
            availW = maxColW;
        }
    }

    const Rect contentBounds(startX, docPad, availW, 0.0f);
    out.contentLocalBounds = contentBounds;

    const keel::f32 bodyFs = (opts.bodyFontSize.has_value() ? *opts.bodyFontSize : theme.GetFontSize(TextRole::BodyMedium)) * effZoom;
    const keel::f32 codeFs = (opts.codeFontSize.has_value() ? *opts.codeFontSize : theme.GetFontSize(TextRole::CodeMedium)) * effZoom;
    const keel::f32 inlineCodeFs = opts.inlineCodeFontSize.has_value()
        ? (*opts.inlineCodeFontSize * effZoom)
        : (bodyFs * 0.92f);
    const keel::f32 lhScale = keel::clamp(opts.lineHeightScale, 0.8f, 2.5f);
    const keel::f32 codeLineH = (canvas ? canvas->GetLineHeight(codeFs) : theme.GetLineHeight(codeFs)) * lhScale;
    const keel::f32 gap = opts.blockGap * s;
    const keel::f32 blkPad = keel::max(4.0f, opts.blockPadding) * s;
    const keel::f32 blkRad = keel::max(0.0f, opts.blockRadius) * s;
    const keel::f32 paraIndent = keel::max(0.0f, opts.paragraphIndent) * s;
    const keel::f32 listStep = keel::max(8.0f, opts.listIndent) * s;

    keel::f32 curY = contentBounds.y;
    bool firstBlock = true;
    out.blocks.reserve(doc.blocks.size());

    for (keel::usize bIdx = 0; bIdx < doc.blocks.size(); ++bIdx) {
        const MarkdownBlock& blk = doc.blocks[bIdx];
        if (!firstBlock) {
            curY += (blk.kind == BlockKind::Heading) ? (gap * 1.25f) : gap;
        }
        firstBlock = false;

        BlockLayout bl{};
        bl.kind = blk.kind;
        bl.astBlockIndex = bIdx;

        switch (blk.kind) {
            case BlockKind::Divider: {
                const keel::f32 h = 8.0f * s;
                bl.localBounds = Rect(contentBounds.x, curY, availW, h);
                curY += h;
                out.totalWidth = keel::max(out.totalWidth, availW);
                break;
            }

            case BlockKind::Heading: {
                const keel::f32 hFs = HeadingFontSize(blk.headingLevel, opts, theme, effZoom);
                const keel::f32 headingInlineCodeFs = opts.inlineCodeFontSize.has_value()
                    ? (inlineCodeFs * (hFs / keel::max(1.0f, bodyFs)) * 0.85f)
                    : (hFs * 0.75f);
                const keel::f32 startY = curY;
                bl.flowLocalOrigin = {contentBounds.x, curY};
                bl.flow = BuildInlineFlowLayout(
                    canvas,
                    blk.spans,
                    availW,
                    hFs,
                    headingInlineCodeFs,
                    lhScale,
                    0.0f,
                    theme,
                    effZoom,
                    FontStyle::Bold
                );
                CommitInlineFlowToDocument(bl.flow, bl.flowLocalOrigin, out.textMap, out.linkRegions);
                curY += bl.flow.height;
                if (opts.headingDividers && blk.headingLevel <= 2) {
                    curY += 6.0f * s;
                    bl.hasHeadingDivider = true;
                    bl.dividerLocalY = curY;
                    curY += 2.0f * s;
                }
                bl.localBounds = Rect(contentBounds.x, startY, availW, curY - startY);
                out.totalWidth = keel::max(out.totalWidth, bl.flow.width);
                break;
            }

            case BlockKind::Paragraph: {
                const keel::f32 startY = curY;
                bl.flowLocalOrigin = {contentBounds.x, curY};
                bl.flow = BuildInlineFlowLayout(
                    canvas,
                    blk.spans,
                    availW,
                    bodyFs,
                    inlineCodeFs,
                    lhScale,
                    paraIndent,
                    theme,
                    effZoom,
                    FontStyle::Regular,
                    opts.align
                );
                CommitInlineFlowToDocument(bl.flow, bl.flowLocalOrigin, out.textMap, out.linkRegions);
                curY += bl.flow.height;
                bl.localBounds = Rect(contentBounds.x, startY, availW, bl.flow.height);
                out.totalWidth = keel::max(out.totalWidth, bl.flow.width);
                break;
            }

            case BlockKind::Blockquote: {
                const keel::f32 padX = blkPad + 2.0f * s;
                const keel::f32 padY = blkPad * 0.7f;
                const keel::f32 innerW = keel::max(40.0f, availW - padX * 2.0f);
                bl.flowLocalOrigin = {contentBounds.x + padX, curY + padY};
                bl.flow = BuildInlineFlowLayout(
                    canvas,
                    blk.spans,
                    innerW,
                    bodyFs,
                    inlineCodeFs,
                    lhScale,
                    0.0f,
                    theme,
                    effZoom,
                    FontStyle::Italic
                );
                CommitInlineFlowToDocument(bl.flow, bl.flowLocalOrigin, out.textMap, out.linkRegions);
                const keel::f32 boxH = bl.flow.height + padY * 2.0f;
                bl.localBounds = Rect(contentBounds.x, curY, availW, boxH);
                bl.boxRadius = blkRad;
                curY += boxH;
                out.totalWidth = keel::max(out.totalWidth, availW);
                break;
            }

            case BlockKind::Alert: {
                const keel::f32 padX = blkPad + 4.0f * s;
                const keel::f32 padY = blkPad * 0.85f;
                const keel::f32 headerH = 20.0f * s;
                const keel::f32 innerW = keel::max(40.0f, availW - padX * 2.0f);
                bl.padX = padX;
                bl.padY = padY;
                bl.headerH = headerH;
                bl.boxRadius = blkRad;
                bl.flowLocalOrigin = {contentBounds.x + padX, curY + padY + headerH + 4.0f * s};
                bl.flow = BuildInlineFlowLayout(
                    canvas,
                    blk.spans,
                    innerW,
                    bodyFs,
                    inlineCodeFs,
                    lhScale,
                    0.0f,
                    theme,
                    effZoom,
                    FontStyle::Regular
                );
                CommitInlineFlowToDocument(bl.flow, bl.flowLocalOrigin, out.textMap, out.linkRegions);
                const keel::f32 boxH = padY * 2.0f + headerH + 4.0f * s + bl.flow.height;
                bl.localBounds = Rect(contentBounds.x, curY, availW, boxH);
                curY += boxH;
                out.totalWidth = keel::max(out.totalWidth, availW);
                break;
            }

            case BlockKind::ListItem: {
                const keel::f32 indent = keel::as<keel::f32>(blk.indentLevel) * listStep + (listStep + 4.0f * s);
                const keel::f32 itemX = contentBounds.x + indent;
                const keel::f32 itemW = keel::max(40.0f, availW - indent);
                const keel::f32 lineH = (canvas ? canvas->GetLineHeight(bodyFs) : theme.GetLineHeight(bodyFs)) * lhScale;
                const keel::f32 markerX = itemX - 18.0f * s;

                if (blk.taskState >= 0) {
                    const keel::f32 cbSz = 14.0f * s;
                    bl.markerLocalRect = Rect(markerX, curY + (lineH - cbSz) * 0.5f, cbSz, cbSz);
                    out.taskRegions.push_back(TaskHitRegion{
                        .localHitRect = bl.markerLocalRect.Inset(-3.0f * s),
                        .taskIndex = blk.taskIndex,
                        .defaultChecked = (blk.taskState == 1)
                    });
                } else if (blk.isOrderedList) {
                    bl.markerLocalRect = Rect(markerX - 2.0f * s, curY + (lineH - bodyFs) * 0.5f, 16.0f * s, bodyFs);
                } else {
                    const keel::f32 dotSz = 5.0f * s;
                    bl.markerLocalRect = Rect(markerX + 4.0f * s, curY + (lineH - dotSz) * 0.5f, dotSz, dotSz);
                }

                bl.flowLocalOrigin = {itemX, curY};
                bl.flow = BuildInlineFlowLayout(
                    canvas,
                    blk.spans,
                    itemW,
                    bodyFs,
                    inlineCodeFs,
                    lhScale,
                    0.0f,
                    theme,
                    effZoom,
                    FontStyle::Regular
                );
                CommitInlineFlowToDocument(bl.flow, bl.flowLocalOrigin, out.textMap, out.linkRegions);
                bl.localBounds = Rect(contentBounds.x, curY, availW, keel::max(lineH, bl.flow.height));
                curY += bl.flow.height;
                out.totalWidth = keel::max(out.totalWidth, indent + bl.flow.width);
                break;
            }

            case BlockKind::CodeBlock: {
                keel::Array<keel::StringRef> codeLines;
                keel::usize p = 0;
                keel::StringRef raw = blk.rawText;
                while (p <= raw.size()) {
                    const keel::usize nl = raw.find('\n', p);
                    if (nl == keel::StringRef::npos) {
                        codeLines.push_back(raw.substr(p));
                        break;
                    }
                    codeLines.push_back(raw.substr(p, nl - p));
                    p = nl + 1;
                }
                if (codeLines.empty()) codeLines.push_back("");

                const keel::f32 headerH = 28.0f * s;
                const keel::f32 padY = blkPad * 0.85f;
                const keel::f32 padX = blkPad;
                const keel::f32 gutterW = opts.codeLineNumbers ? (36.0f * s) : 0.0f;
                const keel::f32 codeStartX = contentBounds.x + gutterW + padX;
                const keel::f32 codeAvailW = keel::max(48.0f * s, availW - gutterW - padX * 2.0f);
                const keel::f32 spaceW = keel::max(1.0f, MeasureStyledSlice(canvas, " ", codeFs, FontStyle::Monospace));
                const keel::f32 codeGlyphH = canvas ? canvas->MeasureTextStyled("Mg", codeFs, FontStyle::Monospace).y : codeFs;

                SyntaxConfig synCfg = SyntaxConfig::ForLanguage(blk.codeLang);
                if (opts.codePalette.has_value()) {
                    synCfg.palette = *opts.codePalette;
                }
                const Color defaultCodeCol = opts.codePalette.has_value()
                    ? opts.codePalette->identifier
                    : theme.palette.textPrimary;

                keel::Array<TextSpan> syntaxSpans;
                bool inBlockComment = false;

                bl.codeVisualLines.reserve(codeLines.size());

                for (keel::usize ln = 0; ln < codeLines.size(); ++ln) {
                    const keel::StringRef lineStr = codeLines[ln];
                    syntaxSpans.clear();
                    if (opts.syntaxHighlight && !lineStr.empty()) {
                        detail::TextArea::HighlightLine(
                            lineStr,
                            ln,
                            synCfg,
                            syntaxSpans,
                            theme,
                            opts.codePalette.has_value() ? &(*opts.codePalette) : nullptr,
                            &inBlockComment
                        );
                    }

                    auto emitSlice = [&](keel::usize startCol, keel::usize len, bool isFirst, bool isLast) {
                        const keel::usize vIdx = bl.codeVisualLines.size();
                        const keel::f32 ly = curY + headerH + padY + keel::as<keel::f32>(vIdx) * codeLineH;
                        const keel::f32 textY = ly + (codeLineH - ((codeGlyphH > 0.0f) ? codeGlyphH : codeFs)) * 0.5f;
                        const keel::StringRef sliceStr = lineStr.substr(startCol, len);
                        const keel::f32 lineW = keel::max(spaceW, MeasureStyledSlice(canvas, sliceStr, codeFs, FontStyle::Monospace));
                        const Rect lineRect(codeStartX, ly, lineW, codeLineH);

                        const keel::usize tokIdx = out.textMap.tokens.size();
                        if (!sliceStr.empty()) {
                            out.textMap.AddToken(lineRect, sliceStr, codeFs, FontStyle::Monospace, 0.0f);
                        } else {
                            out.textMap.AddToken(lineRect, " ", codeFs, FontStyle::Monospace, 0.0f);
                        }
                        if (isLast) {
                            out.textMap.AppendNewline();
                        }

                        CodeVisualLineLayout vline{};
                        vline.logicalLine = ln;
                        vline.isFirstSlice = isFirst;
                        vline.isLastSlice = isLast;
                        vline.sliceStr = sliceStr;
                        vline.localY = ly;
                        vline.textLocalY = textY;
                        vline.tokenIndex = tokIdx;

                        if (!sliceStr.empty()) {
                            if (syntaxSpans.empty()) {
                                vline.renderSpans.push_back(CodeRenderSpan{
                                    .text = sliceStr,
                                    .color = defaultCodeCol,
                                    .localOffsetX = 0.0f
                                });
                            } else {
                                const keel::usize sliceEnd = startCol + len;
                                keel::f32 spanOffX = 0.0f;
                                for (const TextSpan& sp : syntaxSpans) {
                                    const keel::usize spStart = keel::as<keel::usize>(sp.startCol);
                                    const keel::usize spEnd = spStart + keel::as<keel::usize>(sp.length);
                                    const keel::usize segStart = keel::max(spStart, startCol);
                                    const keel::usize segEnd = keel::min(spEnd, keel::min(sliceEnd, lineStr.size()));
                                    if (segEnd > segStart) {
                                        const keel::StringRef tok = lineStr.substr(segStart, segEnd - segStart);
                                        bool allSpaces = true;
                                        for (char ch : tok) {
                                            if (ch != ' ' && ch != '\t') {
                                                allSpaces = false;
                                                break;
                                            }
                                        }
                                        if (!allSpaces) {
                                            vline.renderSpans.push_back(CodeRenderSpan{
                                                .text = tok,
                                                .color = sp.color,
                                                .localOffsetX = spanOffX
                                            });
                                        }
                                        spanOffX += MeasureStyledSlice(canvas, tok, codeFs, FontStyle::Monospace);
                                    }
                                }
                            }
                        }

                        bl.codeVisualLines.push_back(keel::move(vline));
                    };

                    if (lineStr.empty()) {
                        emitSlice(0, 0, true, true);
                        continue;
                    }
                    const keel::f32 fullW = MeasureStyledSlice(canvas, lineStr, codeFs, FontStyle::Monospace);
                    if (fullW <= codeAvailW + 0.5f) {
                        emitSlice(0, lineStr.size(), true, true);
                        continue;
                    }

                    keel::usize pos = 0;
                    while (pos < lineStr.size()) {
                        const keel::usize approxChars = keel::max<keel::usize>(1, keel::as<keel::usize>(keel::floor(codeAvailW / spaceW)));
                        keel::usize fitEnd = keel::min(lineStr.size(), pos + approxChars);
                        while (fitEnd < lineStr.size() && (keel::as<keel::u8>(lineStr[fitEnd]) & 0xC0) == 0x80) {
                            ++fitEnd;
                        }
                        while (fitEnd < lineStr.size()) {
                            keel::usize nextCp = fitEnd + 1;
                            while (nextCp < lineStr.size() && (keel::as<keel::u8>(lineStr[nextCp]) & 0xC0) == 0x80) {
                                ++nextCp;
                            }
                            if (MeasureStyledSlice(canvas, lineStr.substr(pos, nextCp - pos), codeFs, FontStyle::Monospace) <= codeAvailW + 0.5f) {
                                fitEnd = nextCp;
                            } else {
                                break;
                            }
                        }
                        while (fitEnd > pos + 1 && MeasureStyledSlice(canvas, lineStr.substr(pos, fitEnd - pos), codeFs, FontStyle::Monospace) > codeAvailW + 0.5f) {
                            --fitEnd;
                            while (fitEnd > pos + 1 && (keel::as<keel::u8>(lineStr[fitEnd]) & 0xC0) == 0x80) {
                                --fitEnd;
                            }
                        }

                        keel::usize splitEnd = fitEnd;
                        if (fitEnd < lineStr.size() && (fitEnd - pos) >= 10) {
                            const keel::usize minBreak = pos + ((fitEnd - pos) * 60) / 100;
                            for (keel::usize k = fitEnd; k > minBreak; --k) {
                                const char ch = lineStr[k - 1];
                                if (ch == ' ' || ch == ',' || ch == ';' || ch == '(' || ch == '{' || ch == '[') {
                                    splitEnd = k;
                                    break;
                                }
                            }
                        }
                        if (splitEnd <= pos) {
                            splitEnd = keel::min(lineStr.size(), pos + 1);
                            while (splitEnd < lineStr.size() && (keel::as<keel::u8>(lineStr[splitEnd]) & 0xC0) == 0x80) {
                                ++splitEnd;
                            }
                        }

                        emitSlice(pos, splitEnd - pos, (pos == 0), (splitEnd >= lineStr.size()));
                        pos = splitEnd;
                    }
                }

                const keel::f32 bodyH = keel::as<keel::f32>(bl.codeVisualLines.size()) * codeLineH + padY * 2.0f;
                const keel::f32 totalBoxH = headerH + bodyH;
                bl.localBounds = Rect(contentBounds.x, curY, availW, totalBoxH);
                bl.headerH = headerH;
                bl.padX = padX;
                bl.padY = padY;
                bl.boxRadius = blkRad;
                bl.gutterW = gutterW;
                bl.codeStartX = codeStartX;
                bl.spaceW = spaceW;
                bl.codeFs = codeFs;
                bl.codeLineH = codeLineH;
                bl.bodyH = bodyH;

                out.copyRegions.push_back(CopyBtnHitRegion{
                    .boxLocalRect = bl.localBounds,
                    .headerH = headerH,
                    .codeBlockIndex = blk.codeBlockIndex,
                    .rawCode = blk.rawText
                });

                curY += totalBoxH;
                out.totalWidth = keel::max(out.totalWidth, availW);
                break;
            }

            case BlockKind::ImageBlock: {
                const keel::f32 startY = curY;
                Vec2 nat = canvas ? canvas->GetImageSize(blk.rawText) : Vec2{0.0f, 0.0f};
                if (nat.x <= 0.0f || nat.y <= 0.0f) {
                    nat = {320.0f, 160.0f};
                }
                const Vec2 scaledNat = { nat.x * s, nat.y * s };
                const keel::f32 maxH = opts.imageMaxHeight * s;
                const keel::f32 scaleFactor = keel::min(1.0f, keel::min(availW / scaledNat.x, maxH / scaledNat.y));
                const keel::f32 imgW = keel::max(40.0f, scaledNat.x * scaleFactor);
                const keel::f32 imgH = keel::max(30.0f, scaledNat.y * scaleFactor);
                const keel::f32 imgX = contentBounds.x + (availW - imgW) * 0.5f;
                bl.imgLocalRect = Rect(imgX, curY, imgW, imgH);
                bl.imgRadius = opts.imageRadius * s;
                curY += imgH;

                if (!blk.spans.empty()) {
                    const keel::f32 capFs = theme.GetFontSize(TextRole::LabelSmall) * effZoom;
                    const keel::f32 capCodeFs = capFs * 0.92f;
                    curY += 4.0f * s;
                    bl.flowLocalOrigin = {contentBounds.x, curY};
                    bl.flow = BuildInlineFlowLayout(
                        canvas,
                        blk.spans,
                        availW,
                        capFs,
                        capCodeFs,
                        lhScale,
                        0.0f,
                        theme,
                        effZoom,
                        FontStyle::Italic,
                        Align::Center
                    );
                    CommitInlineFlowToDocument(bl.flow, bl.flowLocalOrigin, out.textMap, out.linkRegions);
                    curY += bl.flow.height;
                }
                bl.localBounds = Rect(contentBounds.x, startY, availW, curY - startY);
                out.totalWidth = keel::max(out.totalWidth, availW);
                break;
            }

            case BlockKind::Table: {
                if (blk.tableRows.empty()) break;
                keel::usize numCols = keel::max<keel::usize>(1, blk.tableAligns.size());
                for (const auto& r : blk.tableRows) {
                    numCols = keel::max(numCols, r.size());
                }

                const keel::f32 cellPadX = keel::clamp(availW / (keel::as<keel::f32>(numCols) * 14.0f), 4.5f * s, 10.0f * s);
                const keel::f32 cellPadY = keel::clamp(cellPadX * 0.7f, 4.5f * s, 7.0f * s);

                // 1. Single-pass intrinsic width measurement per cell
                keel::Array<keel::f32> minTextW(numCols, 24.0f * s);
                keel::Array<keel::f32> naturalTextW(numCols, 24.0f * s);

                for (keel::usize r = 0; r < blk.tableRows.size(); ++r) {
                    const FontStyle rowBaseStyle = (r == 0) ? FontStyle::Bold : FontStyle::Regular;
                    const auto& rowCells = blk.tableRows[r];
                    for (keel::usize c = 0; c < numCols && c < rowCells.size(); ++c) {
                        const auto [cellMinTokW, cellNatW] = MeasureInlineSpansIntrinsicWidths(
                            canvas,
                            rowCells[c].spans,
                            bodyFs,
                            inlineCodeFs,
                            rowBaseStyle,
                            s
                        );
                        minTextW[c] = keel::max(minTextW[c], cellMinTokW);
                        naturalTextW[c] = keel::max(naturalTextW[c], cellNatW);
                    }
                }

                keel::f32 sumMinTextW = 0.0f;
                for (keel::usize c = 0; c < numCols; ++c) {
                    minTextW[c] = keel::min(minTextW[c], naturalTextW[c]);
                    sumMinTextW += minTextW[c];
                }

                const keel::f32 totalPadW = keel::as<keel::f32>(numCols) * cellPadX * 2.0f;
                const keel::f32 availTextW = keel::max(32.0f * s, availW - totalPadW);
                const keel::f32 tableFontScale = (availTextW < sumMinTextW * 1.03f && sumMinTextW > 0.0f)
                    ? keel::clamp(availTextW / (sumMinTextW * 1.03f), 0.68f, 1.0f)
                    : 1.0f;
                const keel::f32 tableBodyFs = bodyFs * tableFontScale;
                const keel::f32 tableCodeFs = inlineCodeFs * tableFontScale;
                const keel::f32 tableEffZoom = effZoom * tableFontScale;
                const keel::f32 minRowH = (canvas ? canvas->GetLineHeight(tableBodyFs) : theme.GetLineHeight(tableBodyFs)) * lhScale + cellPadY * 2.0f;

                keel::Array<keel::f32> minColW(numCols, 0.0f);
                keel::Array<keel::f32> naturalColW(numCols, 0.0f);
                keel::f32 sumMinW = 0.0f;
                keel::f32 sumNatW = 0.0f;
                for (keel::usize c = 0; c < numCols; ++c) {
                    minColW[c] = minTextW[c] * tableFontScale + cellPadX * 2.0f;
                    naturalColW[c] = naturalTextW[c] * tableFontScale + cellPadX * 2.0f;
                    sumMinW += minColW[c];
                    sumNatW += naturalColW[c];
                }

                bl.colWidths.assign(numCols, availW / keel::as<keel::f32>(numCols));
                if (availW >= sumNatW && sumNatW > 0.0f) {
                    const keel::f32 extraW = availW - sumNatW;
                    for (keel::usize c = 0; c < numCols; ++c) {
                        bl.colWidths[c] = naturalColW[c] + extraW * (naturalColW[c] / sumNatW);
                    }
                } else if (availW >= sumMinW && sumNatW > sumMinW) {
                    const keel::f32 flexAvail = availW - sumMinW;
                    const keel::f32 flexTotal = sumNatW - sumMinW;
                    for (keel::usize c = 0; c < numCols; ++c) {
                        bl.colWidths[c] = minColW[c] + flexAvail * ((naturalColW[c] - minColW[c]) / flexTotal);
                    }
                } else if (sumMinW > 0.0f) {
                    for (keel::usize c = 0; c < numCols; ++c) {
                        bl.colWidths[c] = availW * (minColW[c] / sumMinW);
                    }
                }

                bl.colX.assign(numCols, contentBounds.x);
                {
                    keel::f32 accumX = contentBounds.x;
                    for (keel::usize c = 0; c < numCols; ++c) {
                        bl.colX[c] = accumX;
                        accumX += bl.colWidths[c];
                    }
                }

                // 2. Build each cell's InlineFlowLayout ONCE, compute rowHeights, and commit tokens at centered Y
                const keel::usize numRows = blk.tableRows.size();
                bl.cellFlows.resize(numRows);
                bl.cellFlowOrigins.resize(numRows);
                for (keel::usize r = 0; r < numRows; ++r) {
                    bl.cellFlows[r].resize(numCols);
                    bl.cellFlowOrigins[r].resize(numCols);
                }
                bl.rowHeights.assign(numRows, minRowH);
                keel::f32 tableH = 0.0f;

                for (keel::usize r = 0; r < numRows; ++r) {
                    const FontStyle cellStyle = (r == 0) ? FontStyle::Bold : FontStyle::Regular;
                    const auto& rowCells = blk.tableRows[r];
                    keel::f32 maxCellH = 0.0f;
                    for (keel::usize c = 0; c < numCols && c < rowCells.size(); ++c) {
                        const Align colAlign = (c < blk.tableAligns.size()) ? blk.tableAligns[c] : Align::Start;
                        const keel::f32 cellInnerW = keel::max(8.0f, bl.colWidths[c] - cellPadX * 2.0f);
                        bl.cellFlows[r][c] = BuildInlineFlowLayout(
                            canvas,
                            rowCells[c].spans,
                            cellInnerW,
                            tableBodyFs,
                            tableCodeFs,
                            lhScale,
                            0.0f,
                            theme,
                            tableEffZoom,
                            cellStyle,
                            colAlign
                        );
                        maxCellH = keel::max(maxCellH, bl.cellFlows[r][c].height);
                    }
                    bl.rowHeights[r] = keel::max(minRowH, maxCellH + cellPadY * 2.0f);
                    tableH += bl.rowHeights[r];
                }

                keel::f32 ry = curY;
                for (keel::usize r = 0; r < numRows; ++r) {
                    const keel::f32 rowH = bl.rowHeights[r];
                    const auto& rowCells = blk.tableRows[r];
                    for (keel::usize c = 0; c < numCols && c < rowCells.size(); ++c) {
                        const keel::f32 cellContentH = bl.cellFlows[r][c].height;
                        const keel::f32 cellContentY = ry + keel::max(cellPadY, (rowH - cellContentH) * 0.5f);
                        const Vec2 cellOrigin{bl.colX[c] + cellPadX, cellContentY};
                        bl.cellFlowOrigins[r][c] = cellOrigin;
                        CommitInlineFlowToDocument(bl.cellFlows[r][c], cellOrigin, out.textMap, out.linkRegions);
                    }
                    ry += rowH;
                }

                bl.localBounds = Rect(contentBounds.x, curY, availW, tableH);
                bl.boxRadius = blkRad;
                bl.padX = cellPadX;
                bl.padY = cellPadY;
                curY += tableH;
                out.totalWidth = keel::max(out.totalWidth, availW);
                break;
            }
        }

        out.blocks.push_back(keel::move(bl));
    }

    out.totalHeight = keel::max(0.0f, curY + docPad);
    out.totalWidth += docPad * 2.0f;
    return layoutPtr;
}

auto& GetMarkdownLayoutCache() {
    thread_local keel::LruCache<keel::u64, keel::Arc<const MarkdownDocumentLayout>, 6> s_layoutCache;
    return s_layoutCache;
}

void ClearMarkdownThreadCaches() noexcept {
    GetMarkdownParseCache().clear();
    GetMarkdownLayoutCache().clear();
}

keel::Arc<const MarkdownDocumentLayout> GetOrBuildMarkdownLayout(
    ICanvas* canvas,
    keel::f32 outerWidth,
    const keel::Arc<const ParsedMarkdownDoc>& doc,
    const MarkdownOptions& opts,
    const UiTheme& theme,
    keel::f32 zoom
) {
    auto& layoutCache = GetMarkdownLayoutCache();
    const keel::f32 effZoom = keel::clamp(zoom, 0.5f, 3.0f);
    const keel::u64 key = ComputeMarkdownLayoutKey(doc->sourceHash, outerWidth, effZoom, opts, theme, canvas != nullptr);

    if (const auto* cached = layoutCache.find(key)) {
        return *cached;
    }

    auto built = BuildMarkdownDocumentLayout(canvas, outerWidth, doc, opts, theme, effZoom, key);
    return layoutCache.put(key, keel::move(built));
}


} // namespace guikit::detail::md_impl
