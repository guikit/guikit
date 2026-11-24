#pragma once

#include "guikit/backend/i_canvas.hpp"
#include "guikit/style/tokens.hpp"
#include "keel/small_array.h"
#include "keel/string.h"

namespace guikit {

class ICanvas;
struct UiTheme;

/// Subpixel tolerance in points for anti-aliasing and rasterizer grid rounding.
inline constexpr keel::f32 kTextSubpixelTolerance = 0.5f;

/// Platform- and backend-agnostic text layout, wrapping, and truncation utility.
class TextUtil {
public:
    /// Measures single-line text width in points using canvas or fallback ratio.
    [[nodiscard]] static keel::f32 MeasureWidth(
        keel::StringRef text,
        keel::f32 fontSize,
        ICanvas* canvas = nullptr
    ) noexcept;

    /// Measures maximum single-line text width across UiText's active string and all its candidate variants.
    template <typename T>
        requires keel::IsSame<T, UiText>
    [[nodiscard]] static keel::f32 MeasureWidth(
        const T& text,
        keel::f32 fontSize,
        ICanvas* canvas = nullptr
    ) noexcept {
        return MeasureUiTextWidth(text, fontSize, canvas);
    }

    [[nodiscard]] static keel::f32 MeasureUiTextWidth(
        const UiText& text,
        keel::f32 fontSize,
        ICanvas* canvas = nullptr
    ) noexcept;

    /// Truncates text with trailing ellipsis ("...") so that it fits within maxWidth.
    /// If text already fits, returns a FixedString copy of text.
    /// If maxWidth cannot even accommodate "...", returns empty string.
    [[nodiscard]] static keel::FixedString<256> TruncateWithEllipsis(
        keel::StringRef text,
        keel::f32 maxWidth,
        keel::f32 fontSize,
        ICanvas* canvas = nullptr,
        FontStyle style = FontStyle::Regular
    );

    /// Splits text into wrapped lines that fit within maxWidth points.
    /// Breaks by words/whitespace, falling back to character-level breaking if a word exceeds maxWidth.
    /// Preserves explicit newline characters ('\n').
    [[nodiscard]] static keel::SmallArray<keel::StringRef, 16> WrapText(
        keel::StringRef text,
        keel::f32 maxWidth,
        keel::f32 fontSize,
        ICanvas* canvas = nullptr
    );

    /// Calculates total height of wrapped text given maxWidth and lineHeight.
    [[nodiscard]] static keel::f32 MeasureWrappedHeight(
        keel::StringRef text,
        keel::f32 maxWidth,
        keel::f32 fontSize,
        keel::f32 lineHeight,
        ICanvas* canvas = nullptr
    );

    /// Calculates total vertical height for a UiText considering its wrap mode, minLines/maxLines reservation,
    /// and candidate variants. Returns 0.0f when empty and minLines == 0.
    [[nodiscard]] static keel::f32 MeasureUiTextHeight(
        const UiText& text,
        keel::f32 maxWidth,
        keel::f32 fontSize,
        keel::f32 lineHeight,
        ICanvas* canvas = nullptr,
        const UiTheme* theme = nullptr
    ) noexcept;

    /// Renders a UiText within `bounds`, applying SingleLine "..." truncation or Word wrapping (with optional maxLines "..." truncation).
    static void DrawUiText(
        ICanvas& canvas,
        const UiText& text,
        Rect bounds,
        keel::f32 fontSize,
        keel::f32 lineHeight,
        Color color,
        keel::f32 alignX = 0.0f,
        keel::f32 singleLineAlignY = 0.5f,
        const UiTheme* theme = nullptr
    );

    /// Measures total horizontal width of a UiAffix (icon + text + innerGap if both are present).
    [[nodiscard]] static keel::f32 MeasureAffixWidth(
        const UiAffix& affix,
        keel::f32 fontSize,
        keel::f32 iconSize,
        keel::f32 innerGap = 4.0f,
        ICanvas* canvas = nullptr
    ) noexcept;

    /// Renders a UiAffix horizontally inside `rowBounds` starting at `rowBounds.x` (respecting iconPlacement)
    /// and returns the total width consumed by the affix.
    static keel::f32 DrawAffix(
        ICanvas& canvas,
        const UiAffix& affix,
        Rect rowBounds,
        keel::f32 fontSize,
        keel::f32 iconSize,
        Color textColor,
        Color iconColor,
        keel::f32 innerGap = 4.0f
    );

    /// Returns the byte index of the previous UTF-8 codepoint boundary before `index`.
    [[nodiscard]] static keel::usize PrevCharBoundary(keel::StringRef text, keel::usize index) noexcept;

    /// Returns the byte index of the next UTF-8 codepoint boundary after `index`.
    [[nodiscard]] static keel::usize NextCharBoundary(keel::StringRef text, keel::usize index) noexcept;

    /// Returns the byte index of the previous word boundary before `index` (for Ctrl+Left / Ctrl+Backspace).
    [[nodiscard]] static keel::usize PrevWordBoundary(keel::StringRef text, keel::usize index) noexcept;

    /// Returns the byte index of the next word boundary after `index` (for Ctrl+Right / Ctrl+Delete).
    [[nodiscard]] static keel::usize NextWordBoundary(keel::StringRef text, keel::usize index) noexcept;

    /// Computes the `[outStart, outEnd]` byte range of the word or token at `index` (for double-click selection).
    static void SelectWordAt(keel::StringRef text, keel::usize index, keel::usize& outStart, keel::usize& outEnd) noexcept;

    /// Hit-tests a horizontal local coordinate `localX` (in points from text start) to the nearest UTF-8 character boundary byte index.
    [[nodiscard]] static keel::usize HitTestIndex(
        keel::StringRef text,
        keel::f32 localX,
        keel::f32 fontSize,
        ICanvas* canvas = nullptr
    ) noexcept;

    /// Finds the first UTF-8 (ASCII + Cyrillic) case-insensitive occurrence of `needle` inside `haystack`.
    /// Returns true and populates `[outBytePos, outBytePos + outByteLen)` when found.
    [[nodiscard]] static bool FindCaseInsensitive(
        keel::StringRef haystack,
        keel::StringRef needle,
        keel::usize& outBytePos,
        keel::usize& outByteLen
    ) noexcept;

    /// Finds the first UTF-8 (ASCII + Cyrillic) case-insensitive occurrence of `needle` inside `haystack`.
    /// Returns the starting byte offset, or `keel::kInvalidIndex` if not found.
    [[nodiscard]] static keel::usize FindCaseInsensitive(
        keel::StringRef haystack,
        keel::StringRef needle
    ) noexcept;

    /// Renders single-line text inside `bounds` (with ellipsis truncation if needed) and draws a rounded
    /// highlight pill behind the matched byte range `[matchPos, matchPos + matchLen)` when `matchPos != keel::kInvalidIndex`.
    /// Because the text is rendered in a single unbroken pass, baseline and kerning remain 100% invariant.
    static void DrawHighlightedTextBox(
        ICanvas& canvas,
        keel::StringRef text,
        Rect bounds,
        keel::f32 fontSize,
        Color textColor,
        keel::usize matchPos,
        keel::usize matchLen,
        Color highlightFill,
        Color highlightBorder = Color::Transparent(),
        keel::f32 pillRadius = 3.0f,
        keel::f32 alignY = 0.5f,
        FontStyle style = FontStyle::Regular
    );
};

} // namespace guikit
