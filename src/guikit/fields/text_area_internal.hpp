#pragma once

#include "guikit/fields/text_area.hpp"
#include <cstdio>
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/node.hpp"
#include "guikit/style/field_frame.hpp"
#include "guikit/style/style_painter.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/surfaces/toolbar.hpp"
#include "guikit/actions/button.hpp"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/format.h"
#include "keel/function_ref.h"
#include "keel/math.h"
#include "keel/memory.h"
#include "keel/number.h"
#include "keel/small_array.h"
#include "keel/static_array.h"
#include "keel/string.h"
#include "keel/utf8.h"

namespace guikit::detail::ta_impl {

inline constexpr keel::usize kExactWrapMaxLines = 4096u;
inline constexpr keel::usize kExactWrapMaxLineBytes = 16384u;
inline constexpr keel::usize kAnalyticWrapChunkLines = 32u;
inline constexpr keel::usize kLongLineFastThresholdBytes = 1024u;
inline constexpr keel::usize kMaxSyntaxLineBytes = 20000u;
inline constexpr keel::usize kMaxStoredSearchMatches = 100000u;
inline constexpr keel::usize kLexerCheckpointStride = 256u;
inline constexpr keel::usize kWindowedFoldThresholdLines = 50000u;
inline constexpr keel::usize kWindowedFoldRadiusLines = 512u;
inline constexpr keel::usize kWindowedFoldBucketLines = 256u;

inline bool IsIdentStart(char c) noexcept {
    return keel::is_ascii_alpha(c) || c == '_';
}

inline bool IsIdentChar(char c) noexcept {
    return keel::is_ascii_alnum(c) || c == '_';
}

inline char ToLowerAscii(char c) noexcept {
    return keel::to_ascii_lower(c);
}

[[nodiscard]] bool IsBuiltInSortedTokenList(keel::ArrayRef<keel::StringRef> list) noexcept;
[[nodiscard]] bool ContainsToken(
    keel::ArrayRef<keel::StringRef> list,
    keel::StringRef token,
    bool caseInsensitive = false
) noexcept;

using LogicalLine = TextAreaLogicalLine;
using VisualLine = TextAreaVisualLine;
using FoldRegion = TextAreaFoldRegion;
using SearchMatch = TextAreaSearchMatch;

/// Zero-overhead read facade unifying Piece-Tree TextDocument and flat StringRef+LogicalLine[] buffers.
struct TextSourceView {
    const TextDocument* doc{nullptr};
    keel::StringRef flatText{};
    keel::ArrayRef<LogicalLine> flatLines{};
    keel::usize flatMaxLineBytes{0};
    bool flatPureAsciiNoTabs{true};

    [[nodiscard]] static TextSourceView FromDocument(const TextDocument& d) noexcept {
        TextSourceView v{};
        v.doc = &d;
        return v;
    }

    [[nodiscard]] static TextSourceView FromFlat(
        keel::StringRef text,
        keel::ArrayRef<LogicalLine> lines,
        keel::usize maxLineBytes,
        bool pureAsciiNoTabs
    ) noexcept {
        TextSourceView v{};
        v.flatText = text;
        v.flatLines = lines;
        v.flatMaxLineBytes = maxLineBytes;
        v.flatPureAsciiNoTabs = pureAsciiNoTabs;
        return v;
    }

    [[nodiscard]] bool IsDocument() const noexcept { return doc != nullptr; }
    [[nodiscard]] bool Empty() const noexcept { return ByteLength() == 0u; }
    [[nodiscard]] keel::usize ByteLength() const noexcept {
        return doc ? doc->ByteLength() : flatText.size();
    }
    [[nodiscard]] keel::usize LineCount() const noexcept {
        return doc ? doc->LineCount() : keel::max<keel::usize>(1u, flatLines.size());
    }
    [[nodiscard]] keel::usize MaxLineByteLength() const noexcept {
        return doc ? doc->MaxLineByteLength() : flatMaxLineBytes;
    }
    [[nodiscard]] bool IsPureAsciiNoTabs() const noexcept {
        return doc ? doc->IsPureAsciiNoTabs() : flatPureAsciiNoTabs;
    }

    [[nodiscard]] TextLineRange LineByteRange(keel::usize line) const noexcept;
    [[nodiscard]] TextLineCol OffsetToLineCol(keel::usize byteOffset) const noexcept;
    [[nodiscard]] keel::usize LineColToOffset(keel::usize line, keel::usize colBytes) const noexcept;
    [[nodiscard]] char ByteAt(keel::usize byteOffset) const noexcept;
    [[nodiscard]] bool RangeIsPureAsciiNoTabs(keel::usize byteOffset, keel::usize byteLen) const noexcept;
    [[nodiscard]] keel::usize SnapToUtf8Boundary(keel::usize byteOffset) const noexcept;

    /// Returns a zero-copy view for flat buffers, or reads at most `maxBytes` into `scratch` for TextDocument.
    [[nodiscard]] keel::StringRef ReadSliceRef(
        keel::usize byteOffset,
        keel::usize maxBytes,
        keel::String& scratch
    ) const;

    /// Returns a zero-copy line slice for flat buffers, or reads at most `maxBytes` into `scratch` for TextDocument.
    [[nodiscard]] keel::StringRef ReadLineSliceRef(
        keel::usize line,
        keel::usize colByteStart,
        keel::usize maxBytes,
        keel::String& scratch
    ) const;

    void ForEachChunkInRange(
        keel::usize byteOffset,
        keel::usize byteLen,
        keel::FunctionRef<bool(keel::StringRef)> visitor
    ) const;
};

struct EditorFontMetrics {
    keel::f32 fontSize{13.0f};
    keel::f32 glyphBoxH{18.0f};
    keel::f32 lineHeight{21.0f};
    keel::f32 textOffsetY{1.5f};
    keel::f32 spaceAdvance{7.8f};
    keel::f32 monoCharAdvance{7.8f};
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
    keel::f64 maxScrollY{0.0};
    keel::f64 maxScrollX{0.0};
    keel::f64 totalContentH{0.0};
    keel::f64 maxLineW{0.0};
    keel::usize totalVisualRows{1};
    bool needVScroll{false};
    bool needHScroll{false};
    bool hasMinimap{false};
};

/// Merged non-overlapping hidden line interval produced by active collapsed folds.
struct HiddenFoldSpan {
    keel::usize hideStart{0};    ///< First hidden 0-based logical line (inclusive)
    keel::usize hideEnd{0};      ///< Last hidden 0-based logical line (inclusive)
    keel::usize hiddenBefore{0}; ///< Cumulative hidden lines strictly before this span
};

/// 2D horizontal window inside a visual row for O(visibleCols) rendering & hit-testing on huge single lines.
struct HorizontalWindow {
    keel::usize colByteStart{0};  ///< Start byte offset relative to visual row start
    keel::usize colByteLen{0};    ///< Byte length of visible slice
    keel::f64 startXOffset{0.0};  ///< Pixel X offset of colByteStart from visual row origin
    bool fastMonospaceAscii{false};
};

/// Scalable Logical<->Visual row projection supporting Identity, Folded (O(log F)),
/// ExactWrap (normal files), AnalyticSmallWrap (10MB single lines), and AnalyticChunkWrap (1M wrapped lines).
struct TextViewProjection {
    enum class Mode : keel::u8 {
        Identity = 0,
        Folded = 1,
        ExactWrap = 2,
        AnalyticSmallWrap = 3,
        AnalyticChunkWrap = 4
    };

    Mode mode{Mode::Identity};
    TextSourceView source{};
    keel::usize totalVisualRows{1};
    keel::usize wrapCols{80};
    keel::SmallArray<HiddenFoldSpan, 16> hiddenSpans{};
    keel::ArrayRef<VisualLine> exactVisualLines{};
    keel::ArrayRef<keel::usize> smallLinePrefix{};
    keel::ArrayRef<keel::usize> chunkPrefix{};

    void BuildHiddenSpans(
        keel::ArrayRef<FoldRegion> foldRegions,
        keel::ArrayRef<keel::usize> foldedLines
    ) noexcept;

    [[nodiscard]] bool IsLineHidden(keel::usize logicalLine) const noexcept;
    [[nodiscard]] keel::usize LineWrapRowCount(keel::usize logicalLine) const noexcept;
    [[nodiscard]] VisualLine VisualRowAt(keel::usize visualRowIdx) const noexcept;
    [[nodiscard]] keel::usize OffsetToVisualRow(keel::usize byteOffset) const noexcept;
    [[nodiscard]] keel::usize LogicalLineToVisualRow(keel::usize logicalLine) const noexcept;
};

/// Builds/updates a TextViewProjection (reusing caches in `state` when available).
void PrepareTextViewProjection(
    const TextSourceView& source,
    bool wordWrap,
    keel::f32 wrapWidth,
    const EditorFontMetrics& metrics,
    FontStyle textStyle,
    ICanvas* canvas,
    keel::ArrayRef<FoldRegion> foldRegions,
    keel::ArrayRef<keel::usize> foldedLines,
    keel::u8 tabSize,
    keel::u64 docCacheKey,
    TextAreaState* state,
    TextViewProjection& outProj,
    keel::Array<VisualLine>* fallbackExactBuf = nullptr,
    keel::Array<keel::usize>* fallbackSmallPrefix = nullptr,
    keel::Array<keel::usize>* fallbackChunkPrefix = nullptr
);

/// Initializes a read-only TextViewProjection in Render from precomputed TextAreaData caches (0 allocs in steady state).
void AttachRenderProjection(
    const TextSourceView& source,
    const TextAreaOptions& opts,
    keel::f32 wrapWidth,
    const EditorFontMetrics& metrics,
    FontStyle textStyle,
    ICanvas* canvas,
    keel::ArrayRef<FoldRegion> foldRegions,
    keel::ArrayRef<keel::usize> foldedLines,
    const TextAreaData* extData,
    TextViewProjection& outProj,
    keel::Array<VisualLine>& fallbackExactBuf,
    keel::Array<keel::usize>& fallbackSmallPrefix,
    keel::Array<keel::usize>& fallbackChunkPrefix
);

[[nodiscard]] HorizontalWindow ComputeHorizontalWindow(
    const TextSourceView& source,
    const VisualLine& vl,
    const EditorFontMetrics& metrics,
    bool monospace,
    keel::f64 scrollX,
    keel::f32 viewportW
) noexcept;

[[nodiscard]] keel::f64 MeasureColumnXInVisualRow(
    const TextSourceView& source,
    const VisualLine& vl,
    keel::usize colInSub,
    const EditorFontMetrics& metrics,
    FontStyle style,
    ICanvas* canvas,
    keel::u8 tabSize,
    keel::String& scratch
) noexcept;

[[nodiscard]] keel::usize HitTestVisualRowX(
    const TextSourceView& source,
    const VisualLine& vl,
    keel::f64 localX,
    const EditorFontMetrics& metrics,
    FontStyle style,
    ICanvas* canvas,
    keel::u8 tabSize,
    keel::String& scratch
) noexcept;

bool ContainsSizeT(keel::ArrayRef<keel::usize> list, keel::usize val) noexcept;
void SortSizeT(keel::MutArrayRef<keel::usize> arr) noexcept;
[[nodiscard]] keel::u64 HashFoldedLines(keel::ArrayRef<keel::usize> foldedLines) noexcept;
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
void SplitLogicalLines(
    keel::StringRef text,
    keel::Array<LogicalLine>& outLines,
    keel::usize* outMaxLineBytes = nullptr,
    bool* outPureAsciiNoTabs = nullptr
);
void ComputeFoldRegions(
    const TextSourceView& source,
    const SyntaxConfig* synCfg,
    keel::Array<FoldRegion>& outRegions,
    keel::usize startLine = 0u,
    keel::usize endLineExclusive = keel::as<keel::usize>(-1),
    keel::ArrayRef<FoldRegion> preservedRegions = {}
);

/// Invalidates cached lexer checkpoints strictly at/after `dirtyLine` (O(1)).
void InvalidateLexerCheckpointsFromLine(TextAreaState& state, keel::usize dirtyLine) noexcept;

/// Resolves exact multiline block-comment state entering `targetLine` using O(1)-amortized checkpoints
/// (1 byte per 256 lines = ~4 KB for 1M lines) with lazy zero-copy streaming fill.
[[nodiscard]] bool ResolveBlockCommentStateAtLine(
    const TextSourceView& source,
    const SyntaxConfig& cfg,
    keel::usize targetLine,
    keel::u64 docKey,
    TextAreaState* state
);
const FoldRegion* FindFoldRegionStartingAt(keel::ArrayRef<FoldRegion> regions, keel::usize lineIdx) noexcept;
bool IsLineFoldedStart(keel::ArrayRef<keel::usize> foldedLines, keel::usize lineIdx) noexcept;
bool IsLineHiddenByFold(
    keel::ArrayRef<FoldRegion> regions,
    keel::ArrayRef<keel::usize> foldedLines,
    keel::usize lineIdx
) noexcept;

void BuildVisualLines(
    const TextSourceView& source,
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
    const TextSourceView& source,
    keel::StringRef query,
    bool matchCase,
    bool wholeWord,
    keel::Array<SearchMatch>& outMatches,
    keel::MutArrayRef<keel::u8> outRulerBuckets,
    bool& outCapped,
    keel::usize maxMatches = kMaxStoredSearchMatches
);

[[nodiscard]] keel::usize FindFirstSearchMatchAtOrAfter(
    keel::ArrayRef<SearchMatch> matches,
    keel::usize byteOffset
) noexcept;

bool FindMatchingBracketPair(
    const TextSourceView& source,
    keel::usize cursorOffset,
    keel::usize& outOpenOffset,
    keel::usize& outCloseOffset
) noexcept;

keel::StringRef ExtractWordAtOffset(
    const TextSourceView& source,
    keel::usize offset,
    keel::String& scratch,
    keel::usize* outStartOffset = nullptr
) noexcept;

keel::StringRef ExtractCodeWithoutLineComment(keel::StringRef rawLine, keel::StringRef lineCommentPrefix) noexcept;
bool HasCodeBeforeOffsetOnLine(keel::StringRef docText, const LogicalLine& ll, keel::usize cursorOffset) noexcept;
keel::usize ResolveTrackedCodeLine(
    keel::StringRef docText,
    keel::ArrayRef<LogicalLine> logicalLines,
    keel::usize requestedLine1,
    keel::StringRef explicitAnchor,
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

Vec2 MeasureEditorBoxFromStats(
    keel::usize lineCount,
    keel::usize maxLineBytes,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 effZoom,
    keel::f32 availableWidth = 0.0f
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
    const TextSourceView& source,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 effZoom,
    keel::f64 scrollX,
    keel::f64 scrollY,
    bool copyFeedbackActive,
    bool searchOpen,
    bool replaceOpen,
    bool gotoOpen,
    keel::ArrayRef<FoldRegion> foldRegions,
    keel::ArrayRef<keel::usize> foldedLines,
    keel::u64 docCacheKey,
    TextAreaState* state,
    const TextAreaData* renderData,
    TextViewProjection& outProj,
    keel::Array<VisualLine>& fallbackExactBuf,
    keel::Array<keel::usize>& fallbackSmallPrefix,
    keel::Array<keel::usize>& fallbackChunkPrefix,
    keel::f32 debugToolbarRatio = -1.0f
);

/// Processes queued text input and keyboard commands directly against a TextDocument in O(log P).
/// Returns true if the document text was mutated during this frame.
bool HandleTextDocumentInput(
    Context& ctx,
    Id id,
    TextDocument& doc,
    TextAreaState& state,
    const TextAreaOptions& opts,
    const TextViewProjection& proj,
    bool& outLostFocus
);

} // namespace guikit::detail::ta_impl
