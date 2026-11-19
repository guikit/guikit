#include "guikit/core/text_util.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/rich_text.hpp"
#include "guikit/core/theme.hpp"
#include "keel/small_array.h"
#include "keel/utf8.h"
#include "keel/math.h"

namespace guikit {

namespace {

[[nodiscard]] inline bool IsUtf8LeadByte(keel::u8 b) noexcept {
    return keel::utf8_is_lead_byte(b);
}

[[nodiscard]] keel::usize NextUtf8CharBoundary(keel::StringRef text, keel::usize index) noexcept {
    return keel::utf8_next_boundary(text, index);
}

[[nodiscard]] keel::usize ClampUtf8PrefixLength(keel::StringRef text, keel::usize maxBytes) noexcept {
    if (text.size() <= maxBytes) {
        return text.size();
    }
    keel::usize len = maxBytes;
    while (len > 0 && !IsUtf8LeadByte(static_cast<keel::u8>(text[len]))) {
        --len;
    }
    return len;
}

[[nodiscard]] float MeasureStyledWidth(
    keel::StringRef text,
    float fontSize,
    ICanvas* canvas,
    FontStyle style
) noexcept {
    if (text.empty() || fontSize <= 0.0f) {
        return 0.0f;
    }
    if (canvas) {
        return (style == FontStyle::Regular)
            ? canvas->MeasureText(text, fontSize).x
            : canvas->MeasureTextStyled(text, fontSize, style).x;
    }
    return static_cast<float>(text.length()) * (fontSize * kDefaultCharAdvanceRatio);
}

} // namespace

float TextUtil::MeasureWidth(
    keel::StringRef text,
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
    if (text.wrap.rich) {
        const float codeFs = fontSize * 0.92f;
        auto measureRich = [&](keel::StringRef sv) -> float {
            if (sv.empty()) return 0.0f;
            return GetOrMeasureRichTextIntrinsicWidths(
                canvas, sv, fontSize, codeFs, FontStyle::Regular, 1.0f
            ).second;
        };
        float maxW = measureRich(text.text);
        for (keel::StringRef var : text.variants) {
            if (!var.empty()) {
                maxW = keel::max(maxW, measureRich(var));
            }
        }
        return maxW;
    }
    float maxW = MeasureWidth(text.text, fontSize, canvas);
    for (keel::StringRef var : text.variants) {
        if (!var.empty()) {
            maxW = keel::max(maxW, MeasureWidth(var, fontSize, canvas));
        }
    }
    return maxW;
}

keel::FixedString<256> TextUtil::TruncateWithEllipsis(
    keel::StringRef text,
    float maxWidth,
    float fontSize,
    ICanvas* canvas,
    FontStyle style
) {
    keel::FixedString<256> out{};
    if (text.empty() || maxWidth <= 0.0f || fontSize <= 0.0f) {
        return out;
    }

    if (MeasureStyledWidth(text, fontSize, canvas, style) <= maxWidth + kTextSubpixelTolerance) {
        const keel::usize safeLen = ClampUtf8PrefixLength(text, keel::FixedString<256>::capacity());
        out.set(keel::StringRef(text.data(), safeLen));
        return out;
    }

    const float dotsW = MeasureStyledWidth("...", fontSize, canvas, style);
    if (dotsW >= maxWidth) {
        return out;
    }

    const float targetW = maxWidth - dotsW;

    // Collect all valid UTF-8 character end offsets
    keel::SmallArray<keel::usize, 64> boundaries;
    boundaries.reserve(text.size() + 1);
    boundaries.push_back(0);
    for (keel::usize i = 1; i <= text.size(); ++i) {
        if (i == text.size() || IsUtf8LeadByte(static_cast<keel::u8>(text[i]))) {
            boundaries.push_back(i);
        }
    }

    // Binary search over character boundaries
    keel::usize low = 0;
    keel::usize high = boundaries.size() - 1;
    keel::usize bestIdx = 0;

    while (low <= high) {
        const keel::usize mid = low + (high - low) / 2;
        const keel::usize len = boundaries[mid];
        const float w = MeasureStyledWidth(text.substr(0, len), fontSize, canvas, style);
        if (w <= targetW) {
            bestIdx = mid;
            low = mid + 1;
        } else {
            if (mid == 0) break;
            high = mid - 1;
        }
    }

    const keel::usize maxPrefix = keel::FixedString<256>::capacity() - 3;
    const keel::usize bestLen = ClampUtf8PrefixLength(text.substr(0, boundaries[bestIdx]), maxPrefix);
    out.set(keel::StringRef(text.data(), bestLen));
    out.append("...");
    return out;
}

keel::SmallArray<keel::StringRef, 16> TextUtil::WrapText(
    keel::StringRef text,
    float maxWidth,
    float fontSize,
    ICanvas* canvas
) {
    keel::SmallArray<keel::StringRef, 16> lines;
    if (text.empty() || maxWidth <= 0.0f || fontSize <= 0.0f) {
        return lines;
    }

    keel::usize lineStart = 0;
    keel::usize i = 0;
    const keel::usize n = text.size();

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

        const keel::usize wordStart = i;
        while (i < n && text[i] != ' ' && text[i] != '\t' && text[i] != '\n') {
            i = NextUtf8CharBoundary(text, i);
        }
        const keel::usize wordEnd = i;

        // Try candidate line from lineStart to wordEnd
        const keel::StringRef candidate = text.substr(lineStart, wordEnd - lineStart);
        const float candW = MeasureWidth(candidate, fontSize, canvas);

        if (candW <= maxWidth) {
            // Word fits on current line, proceed to next word
            continue;
        }

        // Candidate does not fit
        if (wordStart > lineStart) {
            // Push previous content up to end of previous word
            keel::usize prevEnd = wordStart;
            while (prevEnd > lineStart && (text[prevEnd - 1] == ' ' || text[prevEnd - 1] == '\t')) {
                --prevEnd;
            }
            if (prevEnd > lineStart) {
                lines.push_back(text.substr(lineStart, prevEnd - lineStart));
            }
            lineStart = wordStart;

            // Check if word itself fits on the new line
            const keel::StringRef singleWord = text.substr(wordStart, wordEnd - wordStart);
            if (MeasureWidth(singleWord, fontSize, canvas) <= maxWidth) {
                continue;
            }
        }

        // Word is wider than maxWidth by itself: break it character by character
        keel::usize chunkStart = lineStart;
        while (chunkStart < wordEnd) {
            keel::usize chunkEnd = keel::min(NextUtf8CharBoundary(text, chunkStart), wordEnd);
            while (chunkEnd < wordEnd) {
                const keel::usize nextBoundary = keel::min(NextUtf8CharBoundary(text, chunkEnd), wordEnd);
                if (nextBoundary <= chunkEnd) {
                    break;
                }
                if (MeasureWidth(text.substr(chunkStart, nextBoundary - chunkStart), fontSize, canvas) > maxWidth) {
                    break;
                }
                chunkEnd = nextBoundary;
            }

            if (chunkEnd <= chunkStart) {
                chunkEnd = keel::min(NextUtf8CharBoundary(text, chunkStart), wordEnd);
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
    keel::StringRef text,
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
    ICanvas* canvas,
    const UiTheme* theme
) noexcept {
    if (!text.HasContentOrReserve() || fontSize <= 0.0f || lineHeight <= 0.0f) {
        return 0.0f;
    }

    keel::usize linesCount = keel::max<keel::usize>(1, text.wrap.minLines);

    if (text.wrap.rich && text.wrap.IsWordWrap()) {
        static const UiTheme defaultTheme = UiTheme::Dark();
        const UiTheme& activeTheme = theme ? *theme : defaultTheme;
        const float baseLh = canvas ? canvas->GetLineHeight(fontSize) : (fontSize * kDefaultLineHeightMultiplier);
        const float lhScale = (baseLh > 0.0f) ? (lineHeight / baseLh) : 1.0f;
        auto measureRichH = [&](keel::StringRef sv) -> float {
            if (sv.empty()) return 0.0f;
            const float effAvailW = (maxWidth > 0.0f) ? maxWidth : 100000.0f;
            auto flow = GetOrBuildRichTextFlow(canvas, sv, effAvailW, fontSize, fontSize * 0.92f, lhScale, activeTheme);
            return flow->height;
        };
        float maxH = keel::max(static_cast<float>(linesCount) * lineHeight, measureRichH(text.text));
        for (keel::StringRef var : text.variants) {
            if (!var.empty()) {
                maxH = keel::max(maxH, measureRichH(var));
            }
        }
        if (text.wrap.maxLines > 0) {
            maxH = keel::min(maxH, static_cast<float>(text.wrap.maxLines) * lineHeight);
            maxH = keel::max(maxH, static_cast<float>(text.wrap.minLines) * lineHeight);
        }
        return maxH;
    }

    if (text.wrap.IsWordWrap()) {
        auto countLinesFor = [&](keel::StringRef sv) -> keel::usize {
            if (sv.empty()) {
                return 0;
            }
            if (maxWidth > 0.0f) {
                const auto wrapped = WrapText(sv, maxWidth, fontSize, canvas);
                return keel::max<keel::usize>(1, wrapped.size());
            }
            keel::usize count = 1;
            for (char ch : sv) {
                if (ch == '\n') {
                    ++count;
                }
            }
            return count;
        };

        linesCount = keel::max(linesCount, countLinesFor(text.text));
        for (keel::StringRef var : text.variants) {
            linesCount = keel::max(linesCount, countLinesFor(var));
        }

        if (text.wrap.maxLines > 0) {
            linesCount = keel::min<keel::usize>(linesCount, text.wrap.maxLines);
            linesCount = keel::max<keel::usize>(linesCount, text.wrap.minLines);
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
    float singleLineAlignY,
    const UiTheme* theme
) {
    if (text.empty() || bounds.w <= 0.0f || bounds.h <= 0.0f || fontSize <= 0.0f) {
        return;
    }

    const float lh = (lineHeight > 0.0f) ? lineHeight : (fontSize * kDefaultLineHeightMultiplier);

    if (text.wrap.rich) {
        static const UiTheme defaultTheme = UiTheme::Dark();
        const UiTheme& activeTheme = theme ? *theme : defaultTheme;
        const float baseLh = canvas.GetLineHeight(fontSize);
        const float lhScale = (baseLh > 0.0f) ? (lh / baseLh) : 1.0f;
        const Align horizAlign = (alignX < 0.25f) ? Align::Start : ((alignX > 0.75f) ? Align::End : Align::Center);
        const float effAvailW = text.wrap.IsWordWrap() ? bounds.w : 100000.0f;
        auto flow = GetOrBuildRichTextFlow(
            &canvas,
            text.text,
            effAvailW,
            fontSize,
            fontSize * 0.92f,
            lhScale,
            activeTheme,
            1.0f,
            FontStyle::Regular,
            text.wrap.IsWordWrap() ? horizAlign : Align::Start
        );
        float drawY = bounds.y;
        if (!text.wrap.IsWordWrap() && bounds.h > flow->height + kTextSubpixelTolerance) {
            drawY += (bounds.h - flow->height) * singleLineAlignY;
        }
        float drawX = bounds.x;
        if (!text.wrap.IsWordWrap() && horizAlign != Align::Start) {
            drawX += (bounds.w - flow->width) * alignX;
        }
        const bool needsClip = (!text.wrap.IsWordWrap() && flow->width > bounds.w + kTextSubpixelTolerance) ||
                               (text.wrap.maxLines > 0 && flow->height > bounds.h + kTextSubpixelTolerance);
        if (needsClip) {
            canvas.PushClip(bounds);
        }
        RenderInlineFlowLayout(canvas, *flow, {drawX, drawY}, {drawX, drawY}, color, nullptr, nullptr, activeTheme);
        if (needsClip) {
            canvas.PopClip();
        }
        return;
    }

    if (!text.wrap.IsWordWrap()) {
        const Rect drawRect = (text.wrap.minLines > 1 && bounds.h > lh + kTextSubpixelTolerance)
            ? Rect(bounds.x, bounds.y, bounds.w, lh)
            : bounds;
        const float effAlignY = (text.wrap.minLines > 1 && bounds.h > lh + kTextSubpixelTolerance)
            ? 0.5f
            : singleLineAlignY;

        const keel::usize nlPos = text.text.find('\n');
        const keel::StringRef firstLine = (nlPos != keel::kInvalidIndex)
            ? text.text.substr(0, nlPos)
            : text.text;

        if (nlPos != keel::kInvalidIndex) {
            const keel::usize copyLen = ClampUtf8PrefixLength(firstLine, keel::FixedString<256>::capacity() - 3);
            keel::FixedString<256> withDots(firstLine.data(), copyLen);
            withDots.append("...");
            if (MeasureWidth(withDots, fontSize, &canvas) <= drawRect.w + kTextSubpixelTolerance) {
                canvas.DrawTextBox(withDots, drawRect, fontSize, color, alignX, effAlignY);
            } else {
                const auto truncated = TruncateWithEllipsis(firstLine, drawRect.w, fontSize, &canvas);
                if (!truncated.empty()) {
                    canvas.DrawTextBox(truncated, drawRect, fontSize, color, alignX, effAlignY);
                }
            }
            return;
        }

        if (MeasureWidth(firstLine, fontSize, &canvas) <= drawRect.w + kTextSubpixelTolerance) {
            canvas.DrawTextBox(firstLine, drawRect, fontSize, color, alignX, effAlignY);
        } else {
            const auto truncated = TruncateWithEllipsis(firstLine, drawRect.w, fontSize, &canvas);
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

    const keel::usize heightFitLines = (lh > 0.0f)
        ? static_cast<keel::usize>(keel::max(1.0f, keel::floor((bounds.h + kTextSubpixelTolerance) / lh)))
        : lines.size();
    const keel::usize maxAllowedLines = (text.wrap.maxLines > 0)
        ? keel::min<keel::usize>(text.wrap.maxLines, heightFitLines)
        : heightFitLines;
    const keel::usize visibleCount = keel::min(lines.size(), maxAllowedLines);

    for (keel::usize i = 0; i < visibleCount; ++i) {
        const Rect lineRect(bounds.x, bounds.y + static_cast<float>(i) * lh, bounds.w, lh);
        if (i + 1 == visibleCount && lines.size() > visibleCount) {
            const keel::usize startOffset = static_cast<keel::usize>(lines[i].data() - text.text.data());
            const keel::StringRef tail = text.text.substr(startOffset);
            const keel::usize nlPos = tail.find('\n');
            const keel::StringRef paragraphSlice = (nlPos != keel::kInvalidIndex) ? tail.substr(0, nlPos) : tail;

            keel::FixedString<256> truncated;
            if (MeasureWidth(paragraphSlice, fontSize, &canvas) > bounds.w + kTextSubpixelTolerance) {
                truncated = TruncateWithEllipsis(paragraphSlice, bounds.w, fontSize, &canvas);
            } else {
                const keel::usize copyLen = ClampUtf8PrefixLength(paragraphSlice, keel::FixedString<256>::capacity() - 3);
                keel::FixedString<256> withDots(paragraphSlice.data(), copyLen);
                withDots.append("...");
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

keel::usize TextUtil::PrevCharBoundary(keel::StringRef text, keel::usize index) noexcept {
    return keel::utf8_prev_boundary(text, index);
}

keel::usize TextUtil::NextCharBoundary(keel::StringRef text, keel::usize index) noexcept {
    return keel::utf8_next_boundary(text, index);
}

keel::usize TextUtil::PrevWordBoundary(keel::StringRef text, keel::usize index) noexcept {
    return keel::utf8_prev_word_boundary(text, index);
}

keel::usize TextUtil::NextWordBoundary(keel::StringRef text, keel::usize index) noexcept {
    return keel::utf8_next_word_boundary(text, index);
}

void TextUtil::SelectWordAt(keel::StringRef text, keel::usize index, keel::usize& outStart, keel::usize& outEnd) noexcept {
    keel::utf8_word_bounds_at(text, index, outStart, outEnd);
}

keel::usize TextUtil::HitTestIndex(
    keel::StringRef text,
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
    keel::SmallArray<keel::usize, 64> boundaries;
    boundaries.reserve(text.size() + 1);
    boundaries.push_back(0);
    for (keel::usize i = 1; i <= text.size(); ++i) {
        if (i == text.size() || IsUtf8LeadByte(static_cast<keel::u8>(text[i]))) {
            boundaries.push_back(i);
        }
    }
    if (boundaries.size() <= 1) {
        return 0;
    }

    // Binary search for the character interval [boundaries[low], boundaries[low + 1]] containing localX
    keel::usize low = 0;
    keel::usize high = boundaries.size() - 1;
    while (low + 1 < high) {
        const keel::usize mid = low + (high - low) / 2;
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

bool TextUtil::FindCaseInsensitive(
    keel::StringRef haystack,
    keel::StringRef needle,
    keel::usize& outBytePos,
    keel::usize& outByteLen
) noexcept {
    return keel::utf8_find_case_insensitive(haystack, needle, outBytePos, outByteLen);
}

keel::usize TextUtil::FindCaseInsensitive(keel::StringRef haystack, keel::StringRef needle) noexcept {
    return keel::utf8_find_case_insensitive(haystack, needle);
}

void TextUtil::DrawHighlightedTextBox(
    ICanvas& canvas,
    keel::StringRef text,
    Rect bounds,
    float fontSize,
    Color textColor,
    keel::usize matchPos,
    keel::usize matchLen,
    Color highlightFill,
    Color highlightBorder,
    float pillRadius,
    float alignY,
    FontStyle style
) {
    if (text.empty() || bounds.w <= 1.0f || bounds.h <= 1.0f || fontSize <= 0.0f) {
        return;
    }

    const auto truncText = TruncateWithEllipsis(text, bounds.w, fontSize, &canvas, style);
    if (truncText.empty()) {
        return;
    }

    if (matchPos != keel::kInvalidIndex &&
        matchLen > 0 &&
        matchPos < text.size() &&
        highlightFill.a > 0) {
        const keel::usize safeEnd = keel::min(text.size(), matchPos + matchLen);
        const float x0 = bounds.x + (matchPos == 0
            ? 0.0f
            : canvas.MeasureTextStyled(text.substr(0, matchPos), fontSize, style).x);
        const float x1 = bounds.x + canvas.MeasureTextStyled(text.substr(0, safeEnd), fontSize, style).x;

        if (x1 > x0 + 0.5f && x0 < bounds.Right()) {
            const float clampedX1 = keel::min(x1, bounds.Right());
            if (clampedX1 > x0 + 0.5f) {
                const float hlH = keel::round(fontSize * 1.30f);
                const Rect hlRect = Rect(
                    keel::round(x0 - 1.5f),
                    keel::round(bounds.y + (bounds.h - hlH) * alignY),
                    keel::round((clampedX1 - x0) + 3.0f),
                    hlH
                ).Intersect(bounds);
                if (hlRect.w > 0.0f && hlRect.h > 0.0f) {
                    canvas.DrawRoundRect(
                        hlRect,
                        pillRadius,
                        highlightFill,
                        highlightBorder,
                        (highlightBorder.a > 0) ? 1.0f : 0.0f
                    );
                }
            }
        }
    }

    canvas.DrawTextBoxStyled(truncText, bounds, fontSize, textColor, style, 0.0f, alignY);
}

} // namespace guikit
