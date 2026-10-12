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
            size_t chunkEnd = NextUtf8CharBoundary(text, chunkStart);
            while (chunkEnd <= wordEnd) {
                const size_t nextBoundary = NextUtf8CharBoundary(text, chunkEnd);
                if (nextBoundary > wordEnd) {
                    if (MeasureWidth(text.substr(chunkStart, wordEnd - chunkStart), fontSize, canvas) <= maxWidth) {
                        chunkEnd = wordEnd;
                    }
                    break;
                }
                if (MeasureWidth(text.substr(chunkStart, nextBoundary - chunkStart), fontSize, canvas) > maxWidth) {
                    break;
                }
                chunkEnd = nextBoundary;
            }

            if (chunkEnd == chunkStart) {
                chunkEnd = NextUtf8CharBoundary(text, chunkStart);
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

} // namespace guikit
