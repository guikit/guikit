#pragma once

#include "guikit/widgets/text_area.hpp"
#include <cstdio>
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/style_painter.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/panels/toolbar.hpp"
#include "guikit/widgets/button.hpp"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/format.h"
#include "keel/math.h"
#include "keel/memory.h"
#include "keel/number.h"
#include "keel/small_array.h"
#include "keel/static_array.h"
#include "keel/string.h"
#include "keel/utf8.h"


namespace guikit::detail::ta_impl {

inline bool IsIdentStart(char c) noexcept {
    return keel::is_ascii_alpha(c) || c == '_';
}

inline bool IsIdentChar(char c) noexcept {
    return keel::is_ascii_alnum(c) || c == '_';
}

inline char ToLowerAscii(char c) noexcept {
    return keel::to_ascii_lower(c);
}

inline bool ContainsToken(keel::ArrayRef<keel::StringRef> list, keel::StringRef token) noexcept {
    for (keel::StringRef item : list) {
        if (item == token) return true;
    }
    return false;
}

using LogicalLine = TextAreaLogicalLine;
using VisualLine = TextAreaVisualLine;
using FoldRegion = TextAreaFoldRegion;

struct SearchMatch {
    keel::usize startOffset{0};
    keel::usize endOffset{0};
    keel::usize logicalLine{0};
    keel::usize startCol{0};
    keel::usize endCol{0};
};

struct EditorFontMetrics {
    keel::f32 fontSize{13.0f};
    keel::f32 glyphBoxH{18.0f};
    keel::f32 lineHeight{21.0f};
    keel::f32 textOffsetY{1.5f};
    keel::f32 spaceAdvance{7.8f};
    keel::f32 tabAdvance{31.2f};
    keel::f32 digitAdvance{7.8f};
};


struct SearchOverlayRects {
    Rect panelRect{};
    Rect toggleReplaceBtn{};
    Rect findInputRect{};
    Rect caseBtnRect{};
    Rect wordBtnRect{};
    Rect countRect{};
    Rect prevBtnRect{};
    Rect nextBtnRect{};
    Rect closeBtnRect{};
    Rect replaceInputRect{};
    Rect replaceOneBtnRect{};
    Rect replaceAllBtnRect{};
};

struct DebugToolbarRects {
    Rect panelRect{};
    Rect gripRect{};
    keel::f32 minBarX{0.0f};
    keel::f32 maxBarX{0.0f};
    keel::StaticArray<Rect, 6> btnRects{}; ///< Continue, StepOver, StepInto, StepOut, Restart, Stop
};

struct EditorLayoutRects {
    Rect editorBox{};
    Rect innerBounds{};
    Rect statusBarRect{};
    Rect gutterRect{};
    Rect minimapRect{};
    Rect textViewport{};
    Rect vTrackRect{};
    Rect vThumbRect{};
    Rect vStartBtnRect{};
    Rect vEndBtnRect{};
    bool vHasButtons{false};
    Rect hTrackRect{};
    Rect hThumbRect{};
    Rect hStartBtnRect{};
    Rect hEndBtnRect{};
    bool hHasButtons{false};
    Rect copyBtnRect{};
    Rect searchBtnRect{};
    Rect zoomBtnRect{};
    SearchOverlayRects searchOverlay{};
    DebugToolbarRects debugToolbar{};
    UiSize toolbarSize{UiSize::Sm};
    EditorFontMetrics metrics{};
    keel::f32 fontSize{13.0f};
    keel::f32 glyphBoxH{18.0f};
    keel::f32 lineHeight{18.0f};
    keel::f32 gutterWidth{0.0f};
    keel::f32 bpColWidth{0.0f};
    keel::f32 foldColWidth{0.0f};
    keel::f32 minimapWidth{0.0f};
    keel::f32 statusBarHeight{0.0f};
    keel::f32 maxScrollY{0.0f};
    keel::f32 maxScrollX{0.0f};
    keel::f32 totalContentH{0.0f};
    keel::f32 maxLineW{0.0f};
    bool needVScroll{false};
    bool needHScroll{false};
    bool hasMinimap{false};
};


bool ContainsSizeT(keel::ArrayRef<keel::usize> list, keel::usize val) noexcept;
void SortSizeT(keel::MutArrayRef<keel::usize> arr) noexcept;
void StringEraseSpan(keel::String& s, keel::usize pos, keel::usize len);
void StringInsertSpan(keel::String& s, keel::usize pos, keel::StringRef ins);

keel::f32 MeasureChunkNoTabs(
    ICanvas* canvas,
    keel::StringRef chunk,
    keel::f32 fontSize,
    FontStyle style
) noexcept;

keel::f32 MeasureSegmentWidth(
    ICanvas* canvas,
    keel::StringRef seg,
    keel::f32 fontSize,
    FontStyle style,
    keel::u8 tabSize = 4
) noexcept;

keel::usize HitTestColumnInLine(
    ICanvas* canvas,
    keel::StringRef lineSub,
    keel::f32 localX,
    keel::f32 fontSize,
    FontStyle style,
    keel::u8 tabSize = 4
) noexcept;

void SortFoldRegions(keel::MutArrayRef<FoldRegion> arr) noexcept;
void SplitLogicalLines(keel::StringRef text, keel::Array<LogicalLine>& outLines);
void ComputeFoldRegions(
    keel::StringRef text,
    keel::ArrayRef<LogicalLine> logicalLines,
    const SyntaxConfig* synCfg,
    keel::Array<FoldRegion>& outRegions
);
const FoldRegion* FindFoldRegionStartingAt(keel::ArrayRef<FoldRegion> regions, keel::usize lineIdx) noexcept;
bool IsLineFoldedStart(keel::ArrayRef<keel::usize> foldedLines, keel::usize lineIdx) noexcept;
bool IsLineHiddenByFold(
    keel::ArrayRef<FoldRegion> regions,
    keel::ArrayRef<keel::usize> foldedLines,
    keel::usize lineIdx
) noexcept;
void BuildVisualLines(
    keel::StringRef text,
    keel::ArrayRef<LogicalLine> logicalLines,
    bool wordWrap,
    keel::f32 wrapWidth,
    keel::f32 fontSize,
    FontStyle style,
    ICanvas* canvas,
    keel::Array<VisualLine>& outVisual,
    keel::ArrayRef<FoldRegion> foldRegions,
    keel::ArrayRef<keel::usize> foldedLines,
    keel::u8 tabSize = 4
);
void FindAllSearchMatches(
    keel::StringRef text,
    keel::ArrayRef<LogicalLine> logicalLines,
    keel::StringRef query,
    bool matchCase,
    bool wholeWord,
    keel::Array<SearchMatch>& outMatches
);
bool FindMatchingBracketPair(
    keel::StringRef text,
    keel::usize cursorOffset,
    keel::usize& outFirst,
    keel::usize& outSecond
) noexcept;
keel::StringRef ExtractWordAtOffset(keel::StringRef text, keel::usize offset, keel::usize* outStartOffset = nullptr) noexcept;
keel::StringRef ExtractCodeWithoutLineComment(keel::StringRef rawLine, keel::StringRef lineCommentPrefix) noexcept;
bool HasCodeBeforeOffsetOnLine(keel::StringRef docText, const LogicalLine& ll, keel::usize cursorOffset) noexcept;
keel::usize ResolveTrackedCodeLine(
    keel::usize requestedLine1,
    keel::StringRef explicitAnchor,
    keel::StringRef docText,
    keel::ArrayRef<LogicalLine> logicalLines,
    keel::ArrayRef<keel::usize> baselineToCurrentLine,
    keel::ArrayRef<keel::String> baselineSignatures,
    keel::StringRef lineCommentPrefix
) noexcept;
EditorFontMetrics ResolveEditorFontMetrics(
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 baseFontSize,
    keel::f32 effZoom,
    FontStyle style,
    keel::u8 tabSize = 4
) noexcept;
Vec2 MeasureEditorBox(
    keel::StringRef text,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 effZoom,
    keel::f32 availableWidth = 0.0f
) noexcept;
EditorLayoutRects ComputeEditorLayout(
    Rect bounds,
    keel::StringRef text,
    keel::ArrayRef<LogicalLine> logicalLines,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 effZoom,
    keel::f32 scrollX,
    keel::f32 scrollY,
    bool copyFeedbackActive,
    bool searchOpen,
    bool replaceOpen,
    bool gotoOpen,
    keel::ArrayRef<FoldRegion> foldRegions,
    keel::ArrayRef<keel::usize> foldedLines,
    keel::Array<VisualLine>& outVisualLines,
    keel::f32 debugToolbarRatio = -1.0f
);
keel::usize FindVisualLineForOffset(keel::ArrayRef<VisualLine> visualLines, keel::usize offset) noexcept;
keel::usize FindLogicalLineForOffset(keel::ArrayRef<LogicalLine> logicalLines, keel::usize offset) noexcept;
void DrawWavyUnderline(ICanvas& canvas, keel::f32 x0, keel::f32 x1, keel::f32 y, Color col, keel::f32 scale);

} // namespace guikit::detail::ta_impl
