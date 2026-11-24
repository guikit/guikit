#pragma once

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


namespace guikit::detail::md_impl {

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


constexpr Rect OffsetRect(Rect r, Vec2 delta) noexcept {
    return Rect(r.x + delta.x, r.y + delta.y, r.w, r.h);
}

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


keel::StringRef TrimView(keel::StringRef sv) noexcept;
keel::f32 HeadingFontSize(keel::u8 level, const MarkdownOptions& opts, const UiTheme& theme, keel::f32 zoom) noexcept;
Color AlertAccentColor(AlertKind kind, const UiTheme& theme) noexcept;
Icon AlertIcon(AlertKind kind) noexcept;

keel::Arc<const ParsedMarkdownDoc> GetOrParseMarkdown(keel::StringRef source);
void ClearMarkdownThreadCaches() noexcept;
keel::Arc<const MarkdownDocumentLayout> GetOrBuildMarkdownLayout(
    ICanvas* canvas,
    keel::f32 outerWidth,
    const keel::Arc<const ParsedMarkdownDoc>& doc,
    const MarkdownOptions& opts,
    const UiTheme& theme,
    keel::f32 zoom
);

} // namespace guikit::detail::md_impl
