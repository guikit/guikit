#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "guikit/core/Tokens.hpp"

namespace guikit {

class ICanvas;

/// Subpixel tolerance in points for anti-aliasing and rasterizer grid rounding.
inline constexpr float kTextSubpixelTolerance = 0.5f;

/// Platform- and backend-agnostic text layout, wrapping, and truncation utility.
class TextUtil {
public:
    /// Measures single-line text width in points using canvas or fallback ratio.
    [[nodiscard]] static float MeasureWidth(
        std::string_view text,
        float fontSize,
        ICanvas* canvas = nullptr
    ) noexcept;

    /// Measures maximum single-line text width across UiText's active string and all its candidate variants.
    template <std::same_as<UiText> T>
    [[nodiscard]] static float MeasureWidth(
        const T& text,
        float fontSize,
        ICanvas* canvas = nullptr
    ) noexcept {
        return MeasureUiTextWidth(text, fontSize, canvas);
    }

    [[nodiscard]] static float MeasureUiTextWidth(
        const UiText& text,
        float fontSize,
        ICanvas* canvas = nullptr
    ) noexcept;

    /// Truncates text with trailing ellipsis ("...") so that it fits within maxWidth.
    /// If text already fits, returns std::string(text).
    /// If maxWidth cannot even accommodate "...", returns empty string.
    [[nodiscard]] static std::string TruncateWithEllipsis(
        std::string_view text,
        float maxWidth,
        float fontSize,
        ICanvas* canvas = nullptr
    );

    /// Splits text into wrapped lines that fit within maxWidth points.
    /// Breaks by words/whitespace, falling back to character-level breaking if a word exceeds maxWidth.
    /// Preserves explicit newline characters ('\n').
    [[nodiscard]] static std::vector<std::string_view> WrapText(
        std::string_view text,
        float maxWidth,
        float fontSize,
        ICanvas* canvas = nullptr
    );

    /// Calculates total height of wrapped text given maxWidth and lineHeight.
    [[nodiscard]] static float MeasureWrappedHeight(
        std::string_view text,
        float maxWidth,
        float fontSize,
        float lineHeight,
        ICanvas* canvas = nullptr
    );

    /// Calculates total vertical height for a UiText considering its wrap mode, minLines/maxLines reservation,
    /// and candidate variants. Returns 0.0f when empty and minLines == 0.
    [[nodiscard]] static float MeasureUiTextHeight(
        const UiText& text,
        float maxWidth,
        float fontSize,
        float lineHeight,
        ICanvas* canvas = nullptr
    ) noexcept;

    /// Renders a UiText within `bounds`, applying SingleLine "..." truncation or Word wrapping (with optional maxLines "..." truncation).
    static void DrawUiText(
        ICanvas& canvas,
        const UiText& text,
        Rect bounds,
        float fontSize,
        float lineHeight,
        Color color,
        float alignX = 0.0f,
        float singleLineAlignY = 0.5f
    );

    /// Measures total horizontal width of a UiAffix (icon + text + innerGap if both are present).
    [[nodiscard]] static float MeasureAffixWidth(
        const UiAffix& affix,
        float fontSize,
        float iconSize,
        float innerGap = 4.0f,
        ICanvas* canvas = nullptr
    ) noexcept;

    /// Renders a UiAffix horizontally inside `rowBounds` starting at `rowBounds.x` (respecting iconPlacement)
    /// and returns the total width consumed by the affix.
    static float DrawAffix(
        ICanvas& canvas,
        const UiAffix& affix,
        Rect rowBounds,
        float fontSize,
        float iconSize,
        Color textColor,
        Color iconColor,
        float innerGap = 4.0f
    );

    /// Returns the byte index of the previous UTF-8 codepoint boundary before `index`.
    [[nodiscard]] static size_t PrevCharBoundary(std::string_view text, size_t index) noexcept;

    /// Returns the byte index of the next UTF-8 codepoint boundary after `index`.
    [[nodiscard]] static size_t NextCharBoundary(std::string_view text, size_t index) noexcept;

    /// Returns the byte index of the previous word boundary before `index` (for Ctrl+Left / Ctrl+Backspace).
    [[nodiscard]] static size_t PrevWordBoundary(std::string_view text, size_t index) noexcept;

    /// Returns the byte index of the next word boundary after `index` (for Ctrl+Right / Ctrl+Delete).
    [[nodiscard]] static size_t NextWordBoundary(std::string_view text, size_t index) noexcept;

    /// Computes the `[outStart, outEnd]` byte range of the word or token at `index` (for double-click selection).
    static void SelectWordAt(std::string_view text, size_t index, size_t& outStart, size_t& outEnd) noexcept;

    /// Hit-tests a horizontal local coordinate `localX` (in points from text start) to the nearest UTF-8 character boundary byte index.
    [[nodiscard]] static size_t HitTestIndex(
        std::string_view text,
        float localX,
        float fontSize,
        ICanvas* canvas = nullptr
    ) noexcept;
};

} // namespace guikit
