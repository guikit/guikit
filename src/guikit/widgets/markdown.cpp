#include "guikit/widgets/markdown.hpp"
#include <cstdio>
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/rich_text.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/graphics/icon.hpp"
#include "guikit/widgets/image.hpp"
#include "guikit/widgets/text_area.hpp"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/format.h"
#include "keel/hash_map.h"
#include "keel/lru_cache.h"
#include "keel/math.h"
#include "keel/ref.h"
#include "keel/smart_ptr.h"
#include "keel/string.h"

namespace guikit {

namespace {

enum class AlertKind : keel::u8 {
    None = 0,
    Note,
    Tip,
    Important,
    Warning,
    Caution
};

enum class BlockKind : keel::u8 {
    Heading = 0,
    Paragraph,
    Blockquote,
    Alert,
    CodeBlock,
    ListItem,
    ImageBlock,
    Table,
    Divider
};

struct TableCell {
    keel::Array<InlineSpan> spans{};
};

struct MarkdownBlock {
    BlockKind kind{BlockKind::Paragraph};
    keel::u8 headingLevel{1};            // 1..6
    keel::u8 indentLevel{0};             // 0..3 for nested lists
    bool isOrderedList{false};
    keel::u16 listNumber{1};
    keel::i8 taskState{-1};               // -1 = normal bullet, 0 = [ ], 1 = [x]
    keel::i32 taskIndex{-1};                  // 0-based index across task items in document
    AlertKind alertKind{AlertKind::None};
    keel::String alertTitle{};
    keel::String codeLang{};
    keel::String rawText{};             // Code block content or Image path
    keel::String imageAlt{};            // Image caption / alt
    keel::Array<InlineSpan> spans{};
    keel::Array<Align> tableAligns{};
    keel::Array<keel::Array<TableCell>> tableRows{}; // row 0 = header
    keel::i32 codeBlockIndex{-1};
};

struct ParsedMarkdownDoc {
    keel::u64 sourceHash{0};
    keel::Array<MarkdownBlock> blocks{};
};

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

keel::u64 g_lastAnyMarkdownRunSeq = 0;

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

constexpr Rect OffsetRect(Rect r, Vec2 delta) noexcept {
    return Rect(r.x + delta.x, r.y + delta.y, r.w, r.h);
}

keel::f32 HeadingFontSize(keel::u8 level, const MarkdownOptions& opts, const UiTheme& theme, keel::f32 zoom) noexcept {
    const HeadingLevel hl = static_cast<HeadingLevel>(keel::clamp<keel::u8>(level, 1, 6) - 1);
    const keel::f32 base = theme.GetHeading(hl).titleFontSize;
    return base * keel::clamp(opts.headingScale, 0.5f, 2.5f) * zoom;
}

Color AlertAccentColor(AlertKind kind, const UiTheme& theme) noexcept {
    switch (kind) {
        case AlertKind::Note:      return theme.GetVariant(UiVariant::Info).bg;
        case AlertKind::Tip:       return theme.GetVariant(UiVariant::Success).bg;
        case AlertKind::Important: return theme.GetVariant(UiVariant::Primary).bg;
        case AlertKind::Warning:   return theme.GetVariant(UiVariant::Warning).bg;
        case AlertKind::Caution:   return theme.GetVariant(UiVariant::Danger).bg;
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

// --- Phase 2B: Retained Document Layout (in document-local coordinates (0, 0)) ---

struct CopyBtnHitRegion {
    Rect boxLocalRect{};
    keel::f32 headerH{0.0f};
    keel::i32 codeBlockIndex{-1};
    keel::StringRef rawCode{};
};

struct TaskHitRegion {
    Rect localHitRect{};
    keel::i32 taskIndex{-1};
    bool defaultChecked{false};
};

struct CodeRenderSpan {
    keel::StringRef text{};
    Color color{};
    keel::f32 localOffsetX{0.0f};
};

struct CodeVisualLineLayout {
    keel::usize logicalLine{0};
    bool isFirstSlice{true};
    bool isLastSlice{true};
    keel::StringRef sliceStr{};
    keel::f32 localY{0.0f};
    keel::f32 textLocalY{0.0f};
    keel::usize tokenIndex{keel::kInvalidIndex};
    keel::Array<CodeRenderSpan> renderSpans{};
};

struct BlockLayout {
    BlockKind kind{BlockKind::Paragraph};
    keel::usize astBlockIndex{0};
    Rect localBounds{}; // Bounding box in document-local coordinates (0, 0), used for O(1) frustum culling

    InlineFlowLayout flow{};
    Vec2 flowLocalOrigin{};
    bool hasHeadingDivider{false};
    keel::f32 dividerLocalY{0.0f};

    keel::f32 boxRadius{0.0f};
    keel::f32 padX{0.0f};
    keel::f32 padY{0.0f};
    keel::f32 headerH{0.0f};
    Rect markerLocalRect{};

    keel::Array<CodeVisualLineLayout> codeVisualLines{};
    keel::f32 gutterW{0.0f};
    keel::f32 codeStartX{0.0f};
    keel::f32 spaceW{0.0f};
    keel::f32 codeFs{13.0f};
    keel::f32 codeLineH{18.0f};
    keel::f32 bodyH{0.0f};

    Rect imgLocalRect{};
    keel::f32 imgRadius{0.0f};

    keel::Array<keel::f32> colWidths{};
    keel::Array<keel::f32> colX{};
    keel::Array<keel::f32> rowHeights{};
    keel::Array<keel::Array<InlineFlowLayout>> cellFlows{};
    keel::Array<keel::Array<Vec2>> cellFlowOrigins{};
};

struct MarkdownDocumentLayout {
    keel::Arc<const ParsedMarkdownDoc> doc{}; // Keeps backing AST & rawText slices alive while layout lives
    keel::u64 layoutKey{0};
    keel::f32 outerWidth{0.0f};
    keel::f32 totalWidth{0.0f};
    keel::f32 totalHeight{0.0f};
    keel::f32 effZoom{1.0f};
    keel::f32 scale{1.0f};
    Rect contentLocalBounds{};
    DocumentTextMap textMap{};
    keel::Array<BlockLayout> blocks{};
    keel::Array<LinkHitRegion> linkRegions{};
    keel::Array<CopyBtnHitRegion> copyRegions{};
    keel::Array<TaskHitRegion> taskRegions{};
};

struct MarkdownPassResult {
    keel::f32 totalWidth{0.0f};
    keel::f32 totalHeight{0.0f};
    keel::StringRef hoveredLink{};
    Rect hoveredLinkRect{};
    keel::i32 hoveredCopyBlock{-1};
    keel::StringRef hoveredCopyCode{};
    keel::i32 hoveredTaskIndex{-1};
    bool hoveredTaskCurrentState{false};
};

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
                        TextArea::HighlightLine(
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
                    Image::DrawCheckerboard(canvas, imgRect, bl.imgRadius, theme, 10.0f);
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

MarkdownResponse Markdown::Run(
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
        return Markdown::Measure(p->text, mOpts, n->uiSize, canvas ? canvas : p->lastCanvas, th, availableWidth);
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

        // Compute visible viewport rect by intersecting with clipping ancestors (for O(1) block frustum culling)
        Rect visibleRect = ctrlRect;
        for (const Node* anc = n->parent; anc != nullptr; anc = anc->parent) {
            if (anc->clipChildren) {
                visibleRect = visibleRect.Intersect(anc->computedRect);
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

Vec2 Markdown::Measure(
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

void Markdown::Render(
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
