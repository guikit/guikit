#include "guikit/core/TextUtil.hpp"
#include <algorithm>
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Theme.hpp"

namespace guikit {

namespace {

[[nodiscard]] inline bool IsUtf8LeadByte(uint8_t b) noexcept {
    return (b & 0xC0) != 0x80;
}

[[nodiscard]] size_t PrevUtf8CharBoundary(std::string_view text, size_t index) noexcept {
    if (index >= text.size()) index = text.size();
    while (index > 0 && !IsUtf8LeadByte(static_cast<uint8_t>(text[index]))) {
        --index;
    }
    return index;
}

[[nodiscard]] size_t NextUtf8CharBoundary(std::string_view text, size_t index) noexcept {
    if (index >= text.size()) return text.size();
    ++index;
    while (index < text.size() && !IsUtf8LeadByte(static_cast<uint8_t>(text[index]))) {
        ++index;
    }
    return index;
}

} // namespace

float TextUtil::MeasureWidth(
    std::string_view text,
    float fontSize,
    ICanvas* canvas
) noexcept {
    if (text.empty() || fontSize <= 0.0f) {
        return 0.0f;
    }
    if (canvas) {
        return canvas->MeasureText(text, fontSize).x;
    }
    return static_cast<float>(text.length()) * (fontSize * kDefaultCharAdvanceRatio);
}

float TextUtil::MeasureUiTextWidth(
    const UiText& text,
    float fontSize,
    ICanvas* canvas
) noexcept {
    if (fontSize <= 0.0f) {
        return 0.0f;
    }
    float maxW = MeasureWidth(text.text, fontSize, canvas);
    for (std::string_view var : text.variants) {
        if (!var.empty()) {
            maxW = std::max(maxW, MeasureWidth(var, fontSize, canvas));
        }
    }
    return maxW;
}

std::string TextUtil::TruncateWithEllipsis(
    std::string_view text,
    float maxWidth,
    float fontSize,
    ICanvas* canvas
) {
    if (text.empty() || maxWidth <= 0.0f || fontSize <= 0.0f) {
        return "";
    }

    if (MeasureWidth(text, fontSize, canvas) <= maxWidth + kTextSubpixelTolerance) {
        return std::string(text);
    }

    const float dotsW = MeasureWidth("...", fontSize, canvas);
    if (dotsW >= maxWidth) {
        return "";
    }

    const float targetW = maxWidth - dotsW;

    // Collect all valid UTF-8 character end offsets
    std::vector<size_t> boundaries;
    boundaries.reserve(text.size() + 1);
    boundaries.push_back(0);
    for (size_t i = 1; i <= text.size(); ++i) {
        if (i == text.size() || IsUtf8LeadByte(static_cast<uint8_t>(text[i]))) {
            boundaries.push_back(i);
        }
    }

    // Binary search over character boundaries
    size_t low = 0;
    size_t high = boundaries.size() - 1;
    size_t bestIdx = 0;

    while (low <= high) {
        const size_t mid = low + (high - low) / 2;
        const size_t len = boundaries[mid];
        const float w = MeasureWidth(text.substr(0, len), fontSize, canvas);
        if (w <= targetW) {
            bestIdx = mid;
            low = mid + 1;
        } else {
            if (mid == 0) break;
            high = mid - 1;
        }
    }

    const size_t bestLen = boundaries[bestIdx];
    return std::string(text.substr(0, bestLen)) + "...";
}

std::vector<std::string_view> TextUtil::WrapText(
    std::string_view text,
    float maxWidth,
    float fontSize,
    ICanvas* canvas
) {
    std::vector<std::string_view> lines;
    if (text.empty() || maxWidth <= 0.0f || fontSize <= 0.0f) {
        return lines;
    }

    size_t lineStart = 0;
    size_t i = 0;
    const size_t n = text.size();

    while (i < n) {
        // Handle explicit newline
        if (text[i] == '\n') {
            lines.push_back(text.substr(lineStart, i - lineStart));
            ++i;
            lineStart = i;
            continue;
        }

        // Find next word
        while (i < n && (text[i] == ' ' || text[i] == '\t')) {
            ++i;
        }
        if (i >= n || text[i] == '\n') {
            continue;
        }

        const size_t wordStart = i;
        while (i < n && text[i] != ' ' && text[i] != '\t' && text[i] != '\n') {
            i = NextUtf8CharBoundary(text, i);
        }
        const size_t wordEnd = i;

        // Try candidate line from lineStart to wordEnd
        const std::string_view candidate = text.substr(lineStart, wordEnd - lineStart);
        const float candW = MeasureWidth(candidate, fontSize, canvas);

        if (candW <= maxWidth) {
            // Word fits on current line, proceed to next word
            continue;
        }

        // Candidate does not fit
        if (wordStart > lineStart) {
            // Push previous content up to end of previous word
            size_t prevEnd = wordStart;
            while (prevEnd > lineStart && (text[prevEnd - 1] == ' ' || text[prevEnd - 1] == '\t')) {
                --prevEnd;
            }
            if (prevEnd > lineStart) {
                lines.push_back(text.substr(lineStart, prevEnd - lineStart));
            }
            lineStart = wordStart;

            // Check if word itself fits on the new line
            const std::string_view singleWord = text.substr(wordStart, wordEnd - wordStart);
            if (MeasureWidth(singleWord, fontSize, canvas) <= maxWidth) {
                continue;
            }
        }

        // Word is wider than maxWidth by itself: break it character by character
        size_t chunkStart = lineStart;
        while (chunkStart < wordEnd) {
            size_t chunkEnd = std::min(NextUtf8CharBoundary(text, chunkStart), wordEnd);
            while (chunkEnd < wordEnd) {
                const size_t nextBoundary = std::min(NextUtf8CharBoundary(text, chunkEnd), wordEnd);
                if (nextBoundary <= chunkEnd) {
                    break;
                }
                if (MeasureWidth(text.substr(chunkStart, nextBoundary - chunkStart), fontSize, canvas) > maxWidth) {
                    break;
                }
                chunkEnd = nextBoundary;
            }

            if (chunkEnd <= chunkStart) {
                chunkEnd = std::min(NextUtf8CharBoundary(text, chunkStart), wordEnd);
            }

            if (chunkEnd < wordEnd) {
                lines.push_back(text.substr(chunkStart, chunkEnd - chunkStart));
                chunkStart = chunkEnd;
                lineStart = chunkStart;
            } else {
                lineStart = chunkStart;
                break;
            }
        }
    }

    if (lineStart < n) {
        lines.push_back(text.substr(lineStart));
    }

    return lines;
}

float TextUtil::MeasureWrappedHeight(
    std::string_view text,
    float maxWidth,
    float fontSize,
    float lineHeight,
    ICanvas* canvas
) {
    if (text.empty() || fontSize <= 0.0f || lineHeight <= 0.0f) {
        return 0.0f;
    }
    const auto lines = WrapText(text, maxWidth, fontSize, canvas);
    if (lines.empty()) {
        return lineHeight;
    }
    return static_cast<float>(lines.size()) * lineHeight;
}

float TextUtil::MeasureUiTextHeight(
    const UiText& text,
    float maxWidth,
    float fontSize,
    float lineHeight,
    ICanvas* canvas
) noexcept {
    if (!text.HasContentOrReserve() || fontSize <= 0.0f || lineHeight <= 0.0f) {
        return 0.0f;
    }

    size_t linesCount = std::max<size_t>(1, text.wrap.minLines);

    if (text.wrap.IsWordWrap()) {
        auto countLinesFor = [&](std::string_view sv) -> size_t {
            if (sv.empty()) {
                return 0;
            }
            if (maxWidth > 0.0f) {
                const auto wrapped = WrapText(sv, maxWidth, fontSize, canvas);
                return std::max<size_t>(1, wrapped.size());
            }
            size_t count = 1;
            for (char ch : sv) {
                if (ch == '\n') {
                    ++count;
                }
            }
            return count;
        };

        linesCount = std::max(linesCount, countLinesFor(text.text));
        for (std::string_view var : text.variants) {
            linesCount = std::max(linesCount, countLinesFor(var));
        }

        if (text.wrap.maxLines > 0) {
            linesCount = std::min<size_t>(linesCount, text.wrap.maxLines);
            linesCount = std::max<size_t>(linesCount, text.wrap.minLines);
        }
    }

    return static_cast<float>(linesCount) * lineHeight;
}

void TextUtil::DrawUiText(
    ICanvas& canvas,
    const UiText& text,
    Rect bounds,
    float fontSize,
    float lineHeight,
    Color color,
    float alignX,
    float singleLineAlignY
) {
    if (text.empty() || bounds.w <= 0.0f || bounds.h <= 0.0f || fontSize <= 0.0f) {
        return;
    }

    const float lh = (lineHeight > 0.0f) ? lineHeight : (fontSize * kDefaultLineHeightMultiplier);

    if (!text.wrap.IsWordWrap()) {
        const Rect drawRect = (text.wrap.minLines > 1 && bounds.h > lh + kTextSubpixelTolerance)
            ? Rect(bounds.x, bounds.y, bounds.w, lh)
            : bounds;
        const float effAlignY = (text.wrap.minLines > 1 && bounds.h > lh + kTextSubpixelTolerance)
            ? 0.5f
            : singleLineAlignY;

        const size_t nlPos = text.text.find('\n');
        const std::string_view firstLine = (nlPos != std::string_view::npos)
            ? text.text.substr(0, nlPos)
            : text.text;

        if (nlPos != std::string_view::npos) {
            const std::string withDots = std::string(firstLine) + "...";
            if (MeasureWidth(withDots, fontSize, &canvas) <= drawRect.w + kTextSubpixelTolerance) {
                canvas.DrawTextBox(withDots, drawRect, fontSize, color, alignX, effAlignY);
            } else {
                const std::string truncated = TruncateWithEllipsis(firstLine, drawRect.w, fontSize, &canvas);
                if (!truncated.empty()) {
                    canvas.DrawTextBox(truncated, drawRect, fontSize, color, alignX, effAlignY);
                }
            }
            return;
        }

        if (MeasureWidth(firstLine, fontSize, &canvas) <= drawRect.w + kTextSubpixelTolerance) {
            canvas.DrawTextBox(firstLine, drawRect, fontSize, color, alignX, effAlignY);
        } else {
            const std::string truncated = TruncateWithEllipsis(firstLine, drawRect.w, fontSize, &canvas);
            if (!truncated.empty()) {
                canvas.DrawTextBox(truncated, drawRect, fontSize, color, alignX, effAlignY);
            }
        }
        return;
    }

    // Multi-line Word wrap mode
    const auto lines = WrapText(text.text, bounds.w, fontSize, &canvas);
    if (lines.empty()) {
        return;
    }

    const size_t heightFitLines = (lh > 0.0f)
        ? static_cast<size_t>(std::max(1.0f, std::floor((bounds.h + kTextSubpixelTolerance) / lh)))
        : lines.size();
    const size_t maxAllowedLines = (text.wrap.maxLines > 0)
        ? std::min<size_t>(text.wrap.maxLines, heightFitLines)
        : heightFitLines;
    const size_t visibleCount = std::min(lines.size(), maxAllowedLines);

    for (size_t i = 0; i < visibleCount; ++i) {
        const Rect lineRect(bounds.x, bounds.y + static_cast<float>(i) * lh, bounds.w, lh);
        if (i + 1 == visibleCount && lines.size() > visibleCount) {
            const size_t startOffset = static_cast<size_t>(lines[i].data() - text.text.data());
            const std::string_view tail = text.text.substr(startOffset);
            const size_t nlPos = tail.find('\n');
            const std::string_view paragraphSlice = (nlPos != std::string_view::npos) ? tail.substr(0, nlPos) : tail;

            std::string truncated;
            if (MeasureWidth(paragraphSlice, fontSize, &canvas) > bounds.w + kTextSubpixelTolerance) {
                truncated = TruncateWithEllipsis(paragraphSlice, bounds.w, fontSize, &canvas);
            } else {
                const std::string withDots = std::string(paragraphSlice) + "...";
                if (MeasureWidth(withDots, fontSize, &canvas) <= bounds.w + kTextSubpixelTolerance) {
                    truncated = withDots;
                } else {
                    truncated = TruncateWithEllipsis(paragraphSlice, bounds.w, fontSize, &canvas);
                }
            }
            if (!truncated.empty()) {
                canvas.DrawTextBox(truncated, lineRect, fontSize, color, alignX, 0.5f);
            }
        } else {
            canvas.DrawTextBox(lines[i], lineRect, fontSize, color, alignX, 0.5f);
        }
    }
}

float TextUtil::MeasureAffixWidth(
    const UiAffix& affix,
    float fontSize,
    float iconSize,
    float innerGap,
    ICanvas* canvas
) noexcept {
    const bool hasIcon = affix.icon.IsValid() && iconSize > 0.0f;
    const float textW = affix.HasText() ? MeasureUiTextWidth(affix.text, fontSize, canvas) : 0.0f;
    float w = 0.0f;
    if (hasIcon) {
        w += iconSize;
    }
    if (textW > 0.0f) {
        if (hasIcon) {
            w += innerGap;
        }
        w += textW;
    }
    return w;
}

float TextUtil::DrawAffix(
    ICanvas& canvas,
    const UiAffix& affix,
    Rect rowBounds,
    float fontSize,
    float iconSize,
    Color textColor,
    Color iconColor,
    float innerGap
) {
    const bool hasIcon = affix.icon.IsValid() && iconSize > 0.0f;
    const float textW = affix.HasText() ? MeasureUiTextWidth(affix.text, fontSize, &canvas) : 0.0f;
    if (!hasIcon && textW <= 0.0f) {
        return 0.0f;
    }

    float curX = rowBounds.x;
    if (affix.iconPlacement != Placement::Trailing) {
        if (hasIcon) {
            const Rect icRect(curX, rowBounds.y + (rowBounds.h - iconSize) * 0.5f, iconSize, iconSize);
            affix.icon.Draw(canvas, icRect, iconColor);
            curX += iconSize;
            if (textW > 0.0f) {
                curX += innerGap;
            }
        }
        if (textW > 0.0f) {
            if (!affix.text.empty()) {
                const Rect txtRect(curX, rowBounds.y, textW, rowBounds.h);
                canvas.DrawTextBox(affix.text.text, txtRect, fontSize, textColor, 0.0f, 0.5f);
            }
            curX += textW;
        }
    } else {
        if (textW > 0.0f) {
            if (!affix.text.empty()) {
                const Rect txtRect(curX, rowBounds.y, textW, rowBounds.h);
                canvas.DrawTextBox(affix.text.text, txtRect, fontSize, textColor, 0.0f, 0.5f);
            }
            curX += textW;
            if (hasIcon) {
                curX += innerGap;
            }
        }
        if (hasIcon) {
            const Rect icRect(curX, rowBounds.y + (rowBounds.h - iconSize) * 0.5f, iconSize, iconSize);
            affix.icon.Draw(canvas, icRect, iconColor);
            curX += iconSize;
        }
    }
    return curX - rowBounds.x;
}

namespace {

enum class CharCategory : uint8_t {
    Whitespace = 0,
    Word,
    Punctuation
};

[[nodiscard]] CharCategory ClassifyUtf8Char(std::string_view text, size_t charStart) noexcept {
    if (charStart >= text.size()) {
        return CharCategory::Whitespace;
    }
    const uint8_t b = static_cast<uint8_t>(text[charStart]);
    if (b == ' ' || b == '\t' || b == '\n' || b == '\r') {
        return CharCategory::Whitespace;
    }
    // Treat multi-byte UTF-8 sequences (e.g. Cyrillic, European accents, CJK) and ASCII alnum + '_' as word characters
    if (b >= 0x80 ||
        (b >= 'a' && b <= 'z') ||
        (b >= 'A' && b <= 'Z') ||
        (b >= '0' && b <= '9') ||
        b == '_') {
        return CharCategory::Word;
    }
    return CharCategory::Punctuation;
}

} // namespace

size_t TextUtil::PrevCharBoundary(std::string_view text, size_t index) noexcept {
    if (text.empty() || index == 0) {
        return 0;
    }
    if (index > text.size()) {
        index = text.size();
    }
    --index;
    while (index > 0 && !IsUtf8LeadByte(static_cast<uint8_t>(text[index]))) {
        --index;
    }
    return index;
}

size_t TextUtil::NextCharBoundary(std::string_view text, size_t index) noexcept {
    return NextUtf8CharBoundary(text, index);
}

size_t TextUtil::PrevWordBoundary(std::string_view text, size_t index) noexcept {
    if (text.empty() || index == 0) {
        return 0;
    }
    size_t pos = std::min(index, text.size());

    // 1. Skip preceding whitespace backwards
    while (pos > 0) {
        const size_t prev = PrevCharBoundary(text, pos);
        if (ClassifyUtf8Char(text, prev) != CharCategory::Whitespace) {
            break;
        }
        pos = prev;
    }
    if (pos == 0) {
        return 0;
    }

    // 2. Skip contiguous characters of the same non-whitespace category backwards
    const size_t firstChar = PrevCharBoundary(text, pos);
    const CharCategory targetCat = ClassifyUtf8Char(text, firstChar);
    pos = firstChar;
    while (pos > 0) {
        const size_t prev = PrevCharBoundary(text, pos);
        if (ClassifyUtf8Char(text, prev) != targetCat) {
            break;
        }
        pos = prev;
    }
    return pos;
}

size_t TextUtil::NextWordBoundary(std::string_view text, size_t index) noexcept {
    const size_t n = text.size();
    if (n == 0 || index >= n) {
        return n;
    }
    size_t pos = index;
    while (pos > 0 && !IsUtf8LeadByte(static_cast<uint8_t>(text[pos]))) {
        --pos;
    }

    // 1. If starting on whitespace, skip whitespace first
    while (pos < n && ClassifyUtf8Char(text, pos) == CharCategory::Whitespace) {
        pos = NextCharBoundary(text, pos);
    }
    if (pos >= n) {
        return n;
    }

    // 2. Skip contiguous characters of the same category forwards
    const CharCategory targetCat = ClassifyUtf8Char(text, pos);
    while (pos < n && ClassifyUtf8Char(text, pos) == targetCat) {
        pos = NextCharBoundary(text, pos);
    }
    return pos;
}

void TextUtil::SelectWordAt(std::string_view text, size_t index, size_t& outStart, size_t& outEnd) noexcept {
    if (text.empty()) {
        outStart = 0;
        outEnd = 0;
        return;
    }
    const size_t n = text.size();
    size_t pos = std::min(index, n);
    if (pos == n) {
        pos = PrevCharBoundary(text, n);
    } else {
        while (pos > 0 && !IsUtf8LeadByte(static_cast<uint8_t>(text[pos]))) {
            --pos;
        }
    }

    const CharCategory cat = ClassifyUtf8Char(text, pos);
    size_t start = pos;
    while (start > 0) {
        const size_t prev = PrevCharBoundary(text, start);
        if (ClassifyUtf8Char(text, prev) != cat) {
            break;
        }
        start = prev;
    }

    size_t end = NextCharBoundary(text, pos);
    while (end < n && ClassifyUtf8Char(text, end) == cat) {
        end = NextCharBoundary(text, end);
    }

    outStart = start;
    outEnd = end;
}

size_t TextUtil::HitTestIndex(
    std::string_view text,
    float localX,
    float fontSize,
    ICanvas* canvas
) noexcept {
    if (text.empty() || localX <= 0.0f || fontSize <= 0.0f) {
        return 0;
    }
    const float totalW = MeasureWidth(text, fontSize, canvas);
    if (localX >= totalW) {
        return text.size();
    }

    // Collect valid UTF-8 character boundary indices
    std::vector<size_t> boundaries;
    boundaries.reserve(text.size() + 1);
    boundaries.push_back(0);
    for (size_t i = 1; i <= text.size(); ++i) {
        if (i == text.size() || IsUtf8LeadByte(static_cast<uint8_t>(text[i]))) {
            boundaries.push_back(i);
        }
    }
    if (boundaries.size() <= 1) {
        return 0;
    }

    // Binary search for the character interval [boundaries[low], boundaries[low + 1]] containing localX
    size_t low = 0;
    size_t high = boundaries.size() - 1;
    while (low + 1 < high) {
        const size_t mid = low + (high - low) / 2;
        const float w = MeasureWidth(text.substr(0, boundaries[mid]), fontSize, canvas);
        if (w <= localX) {
            low = mid;
        } else {
            high = mid;
        }
    }

    const float w0 = MeasureWidth(text.substr(0, boundaries[low]), fontSize, canvas);
    const float w1 = MeasureWidth(text.substr(0, boundaries[high]), fontSize, canvas);
    const float midX = 0.5f * (w0 + w1);
    return (localX < midX) ? boundaries[low] : boundaries[high];
}

} // namespace guikit
