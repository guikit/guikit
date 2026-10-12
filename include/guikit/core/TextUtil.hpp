#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

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
};

} // namespace guikit
