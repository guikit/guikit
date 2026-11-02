#include "guikit/core/rich_text.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include "guikit/core/id.hpp"
#include "guikit/core/text_util.hpp"

namespace guikit {

namespace {

constexpr Rect OffsetRect(Rect r, Vec2 delta) noexcept {
    return Rect(r.x + delta.x, r.y + delta.y, r.w, r.h);
}

bool CharEqualsIgnoreCase(char a, char b) noexcept {
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

std::string_view TrimView(std::string_view sv) noexcept {
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r')) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r')) {
        sv.remove_suffix(1);
    }
    return sv;
}

} // namespace

// ============================================================================
// 1. Inline Rich-Text AST Spans & Parser
// ============================================================================

float MeasureStyledSlice(
    ICanvas* canvas,
    std::string_view text,
    float fontSize,
    FontStyle style
) noexcept {
    if (text.empty()) return 0.0f;
    if (canvas) {
        return canvas->MeasureTextStyled(text, fontSize, style).x;
    }
    const float ratio = (style == FontStyle::Monospace) ? kMonospaceCharAdvanceRatio : kDefaultCharAdvanceRatio;
    return static_cast<float>(text.size()) * fontSize * ratio;
}

std::vector<InlineSpan> ParseInlineSpans(
    std::string_view line,
    FontStyle inheritedStyle,
    bool inheritedStrike
) {
    std::vector<InlineSpan> out;
    size_t i = 0;
    const size_t n = line.size();

    std::string cur;
    auto flushCurrent = [&]() {
        if (!cur.empty()) {
            out.push_back(InlineSpan{
                .kind = InlineKind::Text,
                .style = inheritedStyle,
                .strikethrough = inheritedStrike,
                .text = std::move(cur),
                .url = {}
            });
            cur.clear();
        }
    };

    auto appendNested = [&](std::string_view inner, FontStyle addedStyle, bool addedStrike) {
        auto nested = ParseInlineSpans(
            inner,
            CombineFontStyles(inheritedStyle, addedStyle),
            inheritedStrike || addedStrike
        );
        for (auto& sp : nested) {
            out.push_back(std::move(sp));
        }
    };

    while (i < n) {
        // 1. Inline code: `code`
        if (line[i] == '`') {
            const size_t endTick = line.find('`', i + 1);
            if (endTick != std::string_view::npos && endTick > i + 1) {
                flushCurrent();
                out.push_back(InlineSpan{
                    .kind = InlineKind::Code,
                    .style = FontStyle::Monospace,
                    .strikethrough = inheritedStrike,
                    .text = std::string(line.substr(i + 1, endTick - i - 1)),
                    .url = {}
                });
                i = endTick + 1;
                continue;
            }
        }

        // 2. Link: [label](url)
        if (line[i] == '[') {
            const size_t closeBr = line.find(']', i + 1);
            if (closeBr != std::string_view::npos && closeBr + 1 < n && line[closeBr + 1] == '(') {
                const size_t closePar = line.find(')', closeBr + 2);
                if (closePar != std::string_view::npos && closeBr > i + 1) {
                    flushCurrent();
                    const std::string linkUrl(TrimView(line.substr(closeBr + 2, closePar - closeBr - 2)));
                    auto labelSpans = ParseInlineSpans(
                        line.substr(i + 1, closeBr - i - 1),
                        inheritedStyle,
                        inheritedStrike
                    );
                    for (auto& sp : labelSpans) {
                        if (sp.kind == InlineKind::Text) {
                            sp.kind = InlineKind::Link;
                        }
                        sp.url = linkUrl;
                        out.push_back(std::move(sp));
                    }
                    i = closePar + 1;
                    continue;
                }
            }
        }

        // 3. Strikethrough: ~~strike~~
        if (i + 1 < n && line[i] == '~' && line[i + 1] == '~') {
            const size_t endTilde = line.find("~~", i + 2);
            if (endTilde != std::string_view::npos && endTilde > i + 2) {
                flushCurrent();
                appendNested(line.substr(i + 2, endTilde - i - 2), FontStyle::Regular, true);
                i = endTilde + 2;
                continue;
            }
        }

        // 4. BoldItalic: ***bold italic***
        if (i + 2 < n && line[i] == '*' && line[i + 1] == '*' && line[i + 2] == '*') {
            const size_t endStar = line.find("***", i + 3);
            if (endStar != std::string_view::npos && endStar > i + 3) {
                flushCurrent();
                appendNested(line.substr(i + 3, endStar - i - 3), FontStyle::BoldItalic, false);
                i = endStar + 3;
                continue;
            }
        }

        // 5. Bold: **bold**
        if (i + 1 < n && line[i] == '*' && line[i + 1] == '*') {
            const size_t endStar = line.find("**", i + 2);
            if (endStar != std::string_view::npos && endStar > i + 2) {
                flushCurrent();
                appendNested(line.substr(i + 2, endStar - i - 2), FontStyle::Bold, false);
                i = endStar + 2;
                continue;
            }
        }

        // 6. Italic: *italic* (CommonMark flanking: no whitespace immediately inside '*')
        if (line[i] == '*' && i + 1 < n &&
            line[i + 1] != ' ' && line[i + 1] != '\t' && line[i + 1] != '\n') {
            size_t searchFrom = i + 1;
            size_t foundEnd = std::string_view::npos;
            while (searchFrom < n) {
                const size_t cand = line.find('*', searchFrom);
                if (cand == std::string_view::npos) break;
                if (cand + 1 < n && line[cand + 1] == '*') {
                    searchFrom = cand + 2;
                    continue;
                }
                if (cand > i + 1 && line[cand - 1] != ' ' && line[cand - 1] != '\t' && line[cand - 1] != '\n') {
                    foundEnd = cand;
                    break;
                }
                searchFrom = cand + 1;
            }
            if (foundEnd != std::string_view::npos) {
                flushCurrent();
                appendNested(line.substr(i + 1, foundEnd - i - 1), FontStyle::Italic, false);
                i = foundEnd + 1;
                continue;
            }
        }

        cur.push_back(line[i]);
        ++i;
    }

    flushCurrent();
    return out;
}

std::pair<float, float> MeasureInlineSpansIntrinsicWidths(
    ICanvas* canvas,
    const std::vector<InlineSpan>& spans,
    float bodyFs,
    float inlineCodeFs,
    FontStyle rowBaseStyle,
    float scale
) noexcept {
    float minTokenW = 0.0f;
    float unwrappedW = 0.0f;
    const float codePadW = 9.0f * scale;

    for (const auto& sp : spans) {
        if (sp.kind == InlineKind::Code) {
            if (sp.text.empty()) continue;
            const float w = MeasureStyledSlice(canvas, sp.text, inlineCodeFs, FontStyle::Monospace) + codePadW;
            minTokenW = std::max(minTokenW, w);
            unwrappedW += w;
        } else {
            const FontStyle effStyle = CombineFontStyles(rowBaseStyle, sp.style);
            std::string_view txt = sp.text;
            size_t pos = 0;
            while (pos < txt.size()) {
                size_t nextPos = pos;
                const bool isWs = (txt[pos] == ' ' || txt[pos] == '\t' || txt[pos] == '\n');
                if (isWs) {
                    while (nextPos < txt.size() && (txt[nextPos] == ' ' || txt[nextPos] == '\t' || txt[nextPos] == '\n')) ++nextPos;
                } else {
                    while (nextPos < txt.size() && txt[nextPos] != ' ' && txt[nextPos] != '\t' && txt[nextPos] != '\n') ++nextPos;
                }
                const std::string_view slice = txt.substr(pos, nextPos - pos);
                pos = nextPos;
                const float w = MeasureStyledSlice(canvas, slice, bodyFs, effStyle);
                unwrappedW += w;
                if (!isWs) {
                    minTokenW = std::max(minTokenW, w);
                }
            }
        }
    }
    return {minTokenW, unwrappedW};
}

std::pair<float, float> GetOrMeasureRichTextIntrinsicWidths(
    ICanvas* canvas,
    std::string_view richMarkup,
    float bodyFs,
    float inlineCodeFs,
    FontStyle baseStyle,
    float scale
) noexcept {
    if (richMarkup.empty() || bodyFs <= 0.0f) {
        return {0.0f, 0.0f};
    }

    thread_local LruCache<uint64_t, std::pair<float, float>, 256> s_widthCache;
    const int32_t qBodyFs = static_cast<int32_t>(std::round(bodyFs * 10.0f));
    const int32_t qCodeFs = static_cast<int32_t>(std::round(inlineCodeFs * 10.0f));
    const int32_t qScale = static_cast<int32_t>(std::round(scale * 100.0f));

    const uint64_t key = detail::HashArgs(
        richMarkup,
        richMarkup.size(),
        qBodyFs,
        qCodeFs,
        qScale,
        static_cast<uint8_t>(baseStyle),
        canvas != nullptr
    );

    if (const auto* cached = s_widthCache.Find(key)) {
        return *cached;
    }

    const auto spans = ParseInlineSpans(richMarkup);
    const auto measured = MeasureInlineSpansIntrinsicWidths(canvas, spans, bodyFs, inlineCodeFs, baseStyle, scale);
    return s_widthCache.Insert(key, measured);
}

// ============================================================================
// 2. Document Text Map (Selection, Cursor Navigation, Link Hit-Test & Search)
// ============================================================================

void DocumentTextMap::AddToken(Rect r, std::string_view str, float fontSize, FontStyle style, float textPadX) {
    if (str.empty()) return;
    const size_t cStart = plainText.size();
    plainText.append(str);
    const size_t cEnd = plainText.size();

    size_t lineIdx = 0;
    if (!lines.empty() && std::abs(lines.back().top - r.y) < r.h * 0.5f) {
        lineIdx = lines.size() - 1;
        PositionedLine& ln = lines.back();
        ln.top = std::min(ln.top, r.y);
        ln.bottom = std::max(ln.bottom, r.Bottom());
        ln.tokenCount += 1;
        ln.charEnd = cEnd;
    } else {
        lineIdx = lines.size();
        lines.push_back(PositionedLine{
            .top = r.y,
            .bottom = r.Bottom(),
            .firstToken = tokens.size(),
            .tokenCount = 1,
            .charStart = cStart,
            .charEnd = cEnd
        });
    }

    tokens.push_back(PositionedToken{
        .rect = r,
        .charStart = cStart,
        .charEnd = cEnd,
        .lineIndex = lineIdx,
        .fontSize = fontSize,
        .fontStyle = style,
        .textPadX = textPadX
    });
}

void DocumentTextMap::AppendNewline() {
    if (!plainText.empty() && plainText.back() != '\n') {
        plainText.push_back('\n');
        if (!lines.empty()) {
            lines.back().charEnd = plainText.size();
        }
    }
}

std::pair<float, float> SubRangePixelBounds(
    ICanvas* canvas,
    const PositionedToken& tok,
    std::string_view fullPlainText,
    size_t selMin,
    size_t selMax
) {
    const size_t s0 = std::max(selMin, tok.charStart);
    const size_t s1 = std::min(selMax, tok.charEnd);
    if (s0 > s1) return {tok.rect.x, tok.rect.x};

    const std::string_view tokStr = fullPlainText.substr(tok.charStart, tok.charEnd - tok.charStart);
    const size_t rel0 = s0 - tok.charStart;
    const size_t rel1 = s1 - tok.charStart;

    const float baseX = tok.rect.x + tok.textPadX;
    const float x0 = (rel0 == 0)
        ? tok.rect.x
        : (baseX + MeasureStyledSlice(canvas, tokStr.substr(0, rel0), tok.fontSize, tok.fontStyle));
    const float x1 = (rel1 >= tokStr.size())
        ? tok.rect.Right()
        : (baseX + MeasureStyledSlice(canvas, tokStr.substr(0, rel1), tok.fontSize, tok.fontStyle));
    return {x0, std::max(x0, x1)};
}

size_t HitTestDocumentCharOffset(
    const DocumentTextMap& map,
    ICanvas* canvas,
    Vec2 mousePos
) {
    if (map.lines.empty() || map.tokens.empty()) return 0;

    if (mousePos.y < map.lines.front().top) {
        return 0;
    }
    if (mousePos.y > map.lines.back().bottom) {
        return map.plainText.size();
    }

    size_t bestLine = 0;
    float bestDistY = 1e9f;
    for (size_t li = 0; li < map.lines.size(); ++li) {
        const PositionedLine& ln = map.lines[li];
        if (mousePos.y >= ln.top && mousePos.y <= ln.bottom) {
            bestLine = li;
            bestDistY = 0.0f;
            break;
        }
        const float dy = (mousePos.y < ln.top) ? (ln.top - mousePos.y) : (mousePos.y - ln.bottom);
        if (dy < bestDistY) {
            bestDistY = dy;
            bestLine = li;
        }
    }

    const PositionedLine& ln = map.lines[bestLine];
    if (ln.tokenCount == 0) return ln.charStart;

    const PositionedToken& firstTok = map.tokens[ln.firstToken];
    const PositionedToken& lastTok = map.tokens[ln.firstToken + ln.tokenCount - 1];
    if (mousePos.x <= firstTok.rect.x) return firstTok.charStart;
    if (mousePos.x >= lastTok.rect.Right()) return lastTok.charEnd;

    for (size_t k = 0; k < ln.tokenCount; ++k) {
        const PositionedToken& tok = map.tokens[ln.firstToken + k];
        if (mousePos.x <= tok.rect.Right()) {
            if (mousePos.x <= tok.rect.x + tok.textPadX) {
                return tok.charStart;
            }
            const std::string_view tokStr = std::string_view(map.plainText).substr(
                tok.charStart,
                tok.charEnd - tok.charStart
            );
            const float localX = mousePos.x - (tok.rect.x + tok.textPadX);
            size_t curIdx = 0;
            float prevW = 0.0f;
            while (curIdx < tokStr.size()) {
                const size_t nextIdx = TextUtil::NextCharBoundary(tokStr, curIdx);
                const float curW = MeasureStyledSlice(canvas, tokStr.substr(0, nextIdx), tok.fontSize, tok.fontStyle);
                if (localX < (prevW + curW) * 0.5f) {
                    return tok.charStart + curIdx;
                }
                prevW = curW;
                curIdx = nextIdx;
            }
            return tok.charEnd;
        }
    }

    return lastTok.charEnd;
}

size_t FindLineForCharOffset(const DocumentTextMap& map, size_t offset) noexcept {
    if (map.lines.empty()) return 0;
    for (size_t li = 0; li < map.lines.size(); ++li) {
        if (offset >= map.lines[li].charStart && offset <= map.lines[li].charEnd) {
            return li;
        }
    }
    return map.lines.size() - 1;
}

float GetCursorXForOffset(const DocumentTextMap& map, ICanvas* canvas, size_t offset) {
    for (const auto& tok : map.tokens) {
        if (offset >= tok.charStart && offset <= tok.charEnd) {
            return SubRangePixelBounds(canvas, tok, map.plainText, offset, offset).first;
        }
    }
    if (!map.tokens.empty()) {
        if (offset == 0) return map.tokens.front().rect.x;
        return map.tokens.back().rect.Right();
    }
    return 0.0f;
}

std::vector<SearchMatchRange> FindSearchMatches(std::string_view text, std::string_view query) {
    std::vector<SearchMatchRange> matches;
    if (query.empty() || text.size() < query.size()) return matches;

    for (size_t i = 0; i + query.size() <= text.size(); ++i) {
        bool ok = true;
        for (size_t j = 0; j < query.size(); ++j) {
            if (!CharEqualsIgnoreCase(text[i + j], query[j])) {
                ok = false;
                break;
            }
        }
        if (ok) {
            matches.push_back(SearchMatchRange{i, i + query.size()});
            i += query.size() - 1;
        }
    }
    return matches;
}

void DrawTokenSelectionAndSearchHighlights(
    ICanvas& canvas,
    const PositionedToken& tok,
    std::string_view fullPlainText,
    const RenderOverlayState* overlay,
    const UiTheme& theme,
    Vec2 docOrigin
) {
    if (!overlay) return;

    if (overlay->searchMatches && !overlay->searchMatches->empty()) {
        const auto& matches = *overlay->searchMatches;
        for (size_t mi = 0; mi < matches.size(); ++mi) {
            const SearchMatchRange& m = matches[mi];
            if (m.end <= tok.charStart || m.start >= tok.charEnd) continue;
            const auto [x0, x1] = SubRangePixelBounds(&canvas, tok, fullPlainText, m.start, m.end);
            if (x1 > x0) {
                const bool isActive = (static_cast<int>(mi) == overlay->activeSearchMatch);
                const Color hlBg = isActive
                    ? theme.GetVariant(UiVariant::Warning).bg.WithAlpha(140)
                    : theme.GetVariant(UiVariant::Warning).bg.WithAlpha(65);
                const Color hlBorder = isActive
                    ? theme.GetVariant(UiVariant::Warning).bg
                    : Color::Transparent();
                canvas.DrawRoundRect(
                    Rect(docOrigin.x + x0, docOrigin.y + tok.rect.y, x1 - x0, tok.rect.h),
                    2.0f,
                    hlBg,
                    hlBorder,
                    isActive ? 1.0f : 0.0f
                );
            }
        }
    }

    if (overlay->hasSelection && overlay->selMax > tok.charStart && overlay->selMin < tok.charEnd) {
        const auto [x0, x1] = SubRangePixelBounds(&canvas, tok, fullPlainText, overlay->selMin, overlay->selMax);
        if (x1 > x0) {
            canvas.DrawRect(
                Rect(docOrigin.x + x0, docOrigin.y + tok.rect.y, x1 - x0, tok.rect.h),
                theme.palette.accent.WithAlpha(85)
            );
        }
    }
}

void DrawTokenCaretIfPresent(
    ICanvas& canvas,
    const PositionedToken& tok,
    std::string_view fullPlainText,
    const RenderOverlayState* overlay,
    const UiTheme& theme,
    Vec2 docOrigin
) {
    if (!overlay || !overlay->showCaret) return;
    if (overlay->caretOffset >= tok.charStart && overlay->caretOffset <= tok.charEnd) {
        const float cx = SubRangePixelBounds(&canvas, tok, fullPlainText, overlay->caretOffset, overlay->caretOffset).first;
        canvas.DrawLine(
            {docOrigin.x + cx, docOrigin.y + tok.rect.y + 1.0f},
            {docOrigin.x + cx, docOrigin.y + tok.rect.Bottom() - 1.0f},
            theme.palette.accent,
            1.5f
        );
    }
}

// ============================================================================
// 3. Self-Contained Inline Flow Layout Engine & Zero-Measurement Renderer
// ============================================================================

namespace {
struct FlowLine {
    size_t firstAtom{0};
    size_t atomCount{0};
    float indent{0.0f};
    float contentWidth{0.0f};
};
} // namespace

InlineFlowLayout BuildInlineFlowLayout(
    ICanvas* canvas,
    const std::vector<InlineSpan>& spans,
    float maxWidth,
    float baseFontSize,
    float codeBaseFontSize,
    float lineHeightScale,
    float firstLineIndent,
    const UiTheme& theme,
    float zoom,
    FontStyle overrideStyle,
    Align horizontalAlign
) {
    InlineFlowLayout res{};
    if (spans.empty()) return res;

    const float s = theme.Scale(1.0f) * zoom;
    const float rawLineH = canvas ? canvas->GetLineHeight(baseFontSize) : theme.GetLineHeight(baseFontSize);
    const float lineH = rawLineH * std::clamp(lineHeightScale, 0.8f, 2.5f);
    const float maxW = (maxWidth > 0.0f) ? maxWidth : 10000.0f;
    const float codePadX = 4.5f * s;

    res.lineH = lineH;
    res.scale = s;

    // Pack all span text & URLs into res.textPool so all std::string_view slices inside `res.items`
    // are 100% self-contained and never dangle even if caller destroys `spans`.
    size_t totalPoolBytes = 0;
    for (const InlineSpan& sp : spans) {
        totalPoolBytes += sp.text.size() + sp.url.size();
    }
    res.textPool.reserve(totalPoolBytes);

    struct PooledSpanView {
        InlineKind kind{InlineKind::Text};
        FontStyle style{FontStyle::Regular};
        bool strikethrough{false};
        std::string_view text{};
        std::string_view url{};
    };
    std::vector<PooledSpanView> pooledSpans;
    pooledSpans.reserve(spans.size());
    for (const InlineSpan& sp : spans) {
        const size_t tStart = res.textPool.size();
        res.textPool.insert(res.textPool.end(), sp.text.begin(), sp.text.end());
        const std::string_view tView(res.textPool.data() + tStart, sp.text.size());

        const size_t uStart = res.textPool.size();
        res.textPool.insert(res.textPool.end(), sp.url.begin(), sp.url.end());
        const std::string_view uView(res.textPool.data() + uStart, sp.url.size());

        pooledSpans.push_back(PooledSpanView{
            .kind = sp.kind,
            .style = sp.style,
            .strikethrough = sp.strikethrough,
            .text = tView,
            .url = uView
        });
    }

    // 1. Flatten PooledSpanViews into initial atomic runs (words, whitespace, explicit newlines, code pills)
    std::vector<InlineAtom> rawAtoms;
    rawAtoms.reserve(pooledSpans.size() * 3);

    for (const PooledSpanView& span : pooledSpans) {
        if (span.kind == InlineKind::Code) {
            if (span.text.empty()) continue;
            const float textW = MeasureStyledSlice(canvas, span.text, codeBaseFontSize, FontStyle::Monospace);
            rawAtoms.push_back(InlineAtom{
                .kind = InlineKind::Code,
                .style = FontStyle::Monospace,
                .strikethrough = span.strikethrough,
                .isWhitespace = false,
                .text = span.text,
                .url = {},
                .width = textW + codePadX * 2.0f,
                .fontSize = codeBaseFontSize,
                .padX = codePadX
            });
            continue;
        }

        const FontStyle effStyle = CombineFontStyles(overrideStyle, span.style);
        std::string_view txt = span.text;
        size_t pos = 0;
        while (pos < txt.size()) {
            if (txt[pos] == '\n') {
                rawAtoms.push_back(InlineAtom{
                    .kind = InlineKind::Text,
                    .style = effStyle,
                    .strikethrough = false,
                    .isWhitespace = true,
                    .text = txt.substr(pos, 1),
                    .url = {},
                    .width = 0.0f,
                    .fontSize = baseFontSize,
                    .padX = 0.0f
                });
                ++pos;
                continue;
            }
            size_t nextPos = pos;
            const bool isWs = (txt[pos] == ' ');
            if (isWs) {
                while (nextPos < txt.size() && txt[nextPos] == ' ') ++nextPos;
            } else {
                while (nextPos < txt.size() && txt[nextPos] != ' ' && txt[nextPos] != '\n') ++nextPos;
            }
            const std::string_view slice = txt.substr(pos, nextPos - pos);
            pos = nextPos;

            const float w = MeasureStyledSlice(canvas, slice, baseFontSize, effStyle);
            rawAtoms.push_back(InlineAtom{
                .kind = span.kind,
                .style = effStyle,
                .strikethrough = span.strikethrough,
                .isWhitespace = isWs,
                .text = slice,
                .url = span.url,
                .width = w,
                .fontSize = baseFontSize,
                .padX = 0.0f
            });
        }
    }

    if (rawAtoms.empty()) return res;

    // 2. Pack atoms into visual lines, keeping contiguous non-whitespace clusters (e.g. `(` + `code` + `)`) together
    //    and splitting oversized tokens/code pills at delimiter boundaries so nothing overflows maxW or orphans `)`.
    std::vector<InlineAtom> atoms;
    atoms.reserve(rawAtoms.size() + 4);
    std::vector<FlowLine> flowLines;
    FlowLine curLine{};
    curLine.firstAtom = 0;
    curLine.indent = (horizontalAlign == Align::Start) ? std::max(0.0f, firstLineIndent) : 0.0f;
    float curX = curLine.indent;

    auto measureAtomSlice = [&](const InlineAtom& proto, std::string_view slice) -> float {
        if (proto.kind == InlineKind::Code) {
            return MeasureStyledSlice(canvas, slice, proto.fontSize, FontStyle::Monospace) + proto.padX * 2.0f;
        }
        return MeasureStyledSlice(canvas, slice, proto.fontSize, proto.style);
    };

    auto splitAtomForWidth = [&](const InlineAtom& atom, float availW) -> std::pair<InlineAtom, InlineAtom> {
        std::string_view sv = atom.text;
        size_t fitEnd = 1;
        while (fitEnd < sv.size() && (static_cast<unsigned char>(sv[fitEnd]) & 0xC0) == 0x80) {
            ++fitEnd;
        }
        while (fitEnd < sv.size()) {
            size_t nextCp = fitEnd + 1;
            while (nextCp < sv.size() && (static_cast<unsigned char>(sv[nextCp]) & 0xC0) == 0x80) {
                ++nextCp;
            }
            if (nextCp < sv.size() && measureAtomSlice(atom, sv.substr(0, nextCp)) <= availW + 0.5f) {
                fitEnd = nextCp;
            } else {
                break;
            }
        }

        size_t splitPos = fitEnd;
        if (fitEnd >= 4) {
            const size_t minBreak = std::max<size_t>(1, (fitEnd * 45) / 100);
            for (size_t b = fitEnd; b > minBreak; --b) {
                const char ch = sv[b - 1];
                if (ch == '/' || ch == ':' || ch == '.' || ch == '-' || ch == '_') {
                    splitPos = b;
                    break;
                }
            }
        }

        InlineAtom head = atom;
        head.text = sv.substr(0, splitPos);
        head.width = measureAtomSlice(atom, head.text);

        InlineAtom tail = atom;
        tail.text = sv.substr(splitPos);
        tail.width = measureAtomSlice(atom, tail.text);
        return {head, tail};
    };

    auto finalizeCurrentLine = [&]() {
        while (curLine.atomCount > 0 && atoms[curLine.firstAtom + curLine.atomCount - 1].isWhitespace) {
            curLine.atomCount -= 1;
        }
        float w = 0.0f;
        for (size_t k = 0; k < curLine.atomCount; ++k) {
            w += atoms[curLine.firstAtom + k].width;
        }
        curLine.contentWidth = w;
        if (curLine.atomCount > 0) {
            flowLines.push_back(curLine);
        }
        curLine = FlowLine{
            .firstAtom = atoms.size(),
            .atomCount = 0,
            .indent = 0.0f,
            .contentWidth = 0.0f
        };
        curX = 0.0f;
    };

    auto appendAtomToCurrentLine = [&](const InlineAtom& a) {
        if (curLine.atomCount == 0) {
            curLine.firstAtom = atoms.size();
        }
        atoms.push_back(a);
        curLine.atomCount += 1;
        curX += a.width;
        curLine.contentWidth = curX - curLine.indent;
    };

    size_t idx = 0;
    while (idx < rawAtoms.size()) {
        if (rawAtoms[idx].isWhitespace) {
            if (rawAtoms[idx].text == "\n") {
                finalizeCurrentLine();
                ++idx;
                continue;
            }
            if (curLine.atomCount == 0) {
                ++idx;
                continue;
            }
            appendAtomToCurrentLine(rawAtoms[idx]);
            ++idx;
            continue;
        }

        size_t clusterEnd = idx + 1;
        float clusterW = rawAtoms[idx].width;
        while (clusterEnd < rawAtoms.size() && !rawAtoms[clusterEnd].isWhitespace) {
            clusterW += rawAtoms[clusterEnd].width;
            ++clusterEnd;
        }

        if (curLine.atomCount > 0 && curX + clusterW > maxW + 0.5f) {
            finalizeCurrentLine();
        }

        for (size_t k = idx; k < clusterEnd; ++k) {
            InlineAtom curAtom = rawAtoms[k];
            const bool hasTrailingPunct =
                (k + 1 == clusterEnd - 1) &&
                (rawAtoms[k + 1].kind == InlineKind::Text) &&
                (rawAtoms[k + 1].text.size() <= 2) &&
                (rawAtoms[k + 1].width <= maxW * 0.25f);
            const float trailingSuffixW = hasTrailingPunct ? rawAtoms[k + 1].width : 0.0f;

            while (curX + curAtom.width + trailingSuffixW > maxW + 0.5f) {
                const bool onlyOpeningPrefixOnLine =
                    (curLine.atomCount == 1) &&
                    (atoms[curLine.firstAtom].kind == InlineKind::Text) &&
                    (atoms[curLine.firstAtom].text.size() <= 2) &&
                    (atoms[curLine.firstAtom].width <= maxW * 0.25f);

                if (curLine.atomCount > 0 && !onlyOpeningPrefixOnLine) {
                    finalizeCurrentLine();
                    continue;
                }

                if (curAtom.text.size() > 1) {
                    const float availForHead = std::max(8.0f, maxW - curX);
                    auto [head, tail] = splitAtomForWidth(curAtom, availForHead);
                    appendAtomToCurrentLine(head);
                    finalizeCurrentLine();
                    curAtom = tail;
                } else {
                    if (curLine.atomCount > 0) {
                        finalizeCurrentLine();
                    }
                    break;
                }
            }

            appendAtomToCurrentLine(curAtom);
        }

        idx = clusterEnd;
    }

    if (curLine.atomCount > 0) {
        finalizeCurrentLine();
    }
    if (flowLines.empty()) return res;

    // 3. Materialize PositionedAtoms in flow-local coordinates (0, 0) with pre-measured glyph heights
    const float codeGlyphH = canvas ? canvas->MeasureTextStyled("Mg", codeBaseFontSize, FontStyle::Monospace).y : codeBaseFontSize;
    std::array<float, 5> cachedGlyphH{};
    for (uint8_t st = 0; st < 5; ++st) {
        cachedGlyphH[st] = canvas
            ? canvas->MeasureTextStyled("Mg", baseFontSize, static_cast<FontStyle>(st)).y
            : baseFontSize;
    }

    float maxLineW = 0.0f;
    res.items.reserve(atoms.size());

    for (size_t li = 0; li < flowLines.size(); ++li) {
        const FlowLine& fl = flowLines[li];
        const float lineY = static_cast<float>(li) * lineH;
        maxLineW = std::max(maxLineW, fl.indent + fl.contentWidth);

        float drawX = fl.indent;
        if (horizontalAlign == Align::Center && maxWidth > 0.0f) {
            drawX = std::max(0.0f, (maxWidth - fl.contentWidth) * 0.5f);
        } else if (horizontalAlign == Align::End && maxWidth > 0.0f) {
            drawX = std::max(0.0f, maxWidth - fl.contentWidth);
        }

        for (size_t k = 0; k < fl.atomCount; ++k) {
            const InlineAtom& atom = atoms[fl.firstAtom + k];
            if (atom.kind == InlineKind::Code) {
                const float pillH = std::max(atom.fontSize + 4.0f * s, lineH - 2.0f * s);
                const Rect pillRect(
                    drawX,
                    lineY + (lineH - pillH) * 0.5f,
                    atom.width,
                    pillH
                );
                const float codeTextY = pillRect.y + (pillRect.h - ((codeGlyphH > 0.0f) ? codeGlyphH : atom.fontSize)) * 0.5f;
                res.items.push_back(PositionedAtom{
                    .atom = atom,
                    .localRect = pillRect,
                    .textLocalY = codeTextY,
                    .lineIdx = li,
                    .tokenIndex = SIZE_MAX
                });
                drawX += atom.width;
                continue;
            }

            const Rect wordRect(drawX, lineY, atom.width, lineH);
            const uint8_t stIdx = std::min<uint8_t>(4, static_cast<uint8_t>(atom.style));
            const float glyphH = cachedGlyphH[stIdx];
            const float textY = lineY + (lineH - ((glyphH > 0.0f) ? glyphH : atom.fontSize)) * 0.5f;

            res.items.push_back(PositionedAtom{
                .atom = atom,
                .localRect = wordRect,
                .textLocalY = textY,
                .lineIdx = li,
                .tokenIndex = SIZE_MAX
            });
            drawX += atom.width;
        }
    }

    res.width = maxLineW;
    res.height = static_cast<float>(flowLines.size()) * lineH;
    res.lineCount = flowLines.size();
    return res;
}

std::shared_ptr<const InlineFlowLayout> GetOrBuildRichTextFlow(
    ICanvas* canvas,
    std::string_view richMarkup,
    float maxWidth,
    float baseFontSize,
    float codeBaseFontSize,
    float lineHeightScale,
    const UiTheme& theme,
    float zoom,
    FontStyle overrideStyle,
    Align horizontalAlign
) {
    thread_local LruCache<uint64_t, std::shared_ptr<const InlineFlowLayout>, 128> s_flowCache;

    const int32_t qWidth = static_cast<int32_t>(std::round(std::max(0.0f, maxWidth) * 2.0f));
    const int32_t qBaseFs = static_cast<int32_t>(std::round(baseFontSize * 10.0f));
    const int32_t qCodeFs = static_cast<int32_t>(std::round(codeBaseFontSize * 10.0f));
    const int32_t qLhScale = static_cast<int32_t>(std::round(lineHeightScale * 100.0f));
    const int32_t qZoom = static_cast<int32_t>(std::round(zoom * 100.0f));
    const int32_t qThemeScale = static_cast<int32_t>(std::round(theme.scale * 100.0f));

    const uint64_t key = detail::HashArgs(
        richMarkup,
        richMarkup.size(),
        qWidth,
        qBaseFs,
        qCodeFs,
        qLhScale,
        qZoom,
        qThemeScale,
        static_cast<uint8_t>(overrideStyle),
        static_cast<uint8_t>(horizontalAlign),
        static_cast<uint8_t>(theme.mode),
        canvas != nullptr
    );

    if (const auto* found = s_flowCache.Find(key)) {
        return *found;
    }

    const auto spans = ParseInlineSpans(richMarkup);
    auto flowPtr = std::make_shared<InlineFlowLayout>(
        BuildInlineFlowLayout(
            canvas,
            spans,
            maxWidth,
            baseFontSize,
            codeBaseFontSize,
            lineHeightScale,
            0.0f,
            theme,
            zoom,
            overrideStyle,
            horizontalAlign
        )
    );
    return s_flowCache.Insert(key, std::move(flowPtr));
}

void CommitInlineFlowToDocument(
    InlineFlowLayout& flow,
    Vec2 flowDocOrigin,
    DocumentTextMap& textMap,
    std::vector<LinkHitRegion>& linkRegions
) {
    if (flow.items.empty()) return;
    size_t currentLine = flow.items.front().lineIdx;
    for (PositionedAtom& item : flow.items) {
        if (item.lineIdx != currentLine) {
            textMap.AppendNewline();
            currentLine = item.lineIdx;
        }
        const Rect docRect = OffsetRect(item.localRect, flowDocOrigin);
        item.tokenIndex = textMap.tokens.size();
        if (item.atom.kind == InlineKind::Code) {
            textMap.AddToken(docRect, item.atom.text, item.atom.fontSize, FontStyle::Monospace, item.atom.padX);
        } else {
            textMap.AddToken(docRect, item.atom.text, item.atom.fontSize, item.atom.style, 0.0f);
            if (item.atom.kind == InlineKind::Link) {
                linkRegions.push_back(LinkHitRegion{
                    .localRect = docRect,
                    .url = item.atom.url
                });
            }
        }
    }
    textMap.AppendNewline();
}

void RenderInlineFlowLayout(
    ICanvas& canvas,
    const InlineFlowLayout& flow,
    Vec2 flowScreenOrigin,
    Vec2 docScreenOrigin,
    Color baseColor,
    const DocumentTextMap* textMap,
    const RenderOverlayState* overlay,
    const UiTheme& theme,
    Vec2 localMousePos
) {
    const float s = flow.scale;
    for (const PositionedAtom& item : flow.items) {
        const InlineAtom& atom = item.atom;
        const Rect screenRect = OffsetRect(item.localRect, flowScreenOrigin);
        const float screenTextY = flowScreenOrigin.y + item.textLocalY;
        const PositionedToken* tok = (textMap != nullptr && item.tokenIndex < textMap->tokens.size())
            ? &textMap->tokens[item.tokenIndex]
            : nullptr;

        if (atom.kind == InlineKind::Code) {
            canvas.DrawRoundRect(
                screenRect,
                4.0f * s,
                theme.palette.surfaceElevated,
                theme.palette.borderSubtle,
                1.0f
            );
            if (tok && textMap) {
                DrawTokenSelectionAndSearchHighlights(canvas, *tok, textMap->plainText, overlay, theme, docScreenOrigin);
            }
            canvas.DrawTextStyled(
                atom.text,
                {screenRect.x + atom.padX, screenTextY},
                atom.fontSize,
                theme.palette.accent,
                FontStyle::Monospace
            );
            if (atom.strikethrough) {
                const float sy = screenRect.y + screenRect.h * 0.52f;
                canvas.DrawLine(
                    {screenRect.x + atom.padX, sy},
                    {screenRect.Right() - atom.padX, sy},
                    theme.palette.accent.WithAlpha(180),
                    1.2f
                );
            }
            if (tok && textMap) {
                DrawTokenCaretIfPresent(canvas, *tok, textMap->plainText, overlay, theme, docScreenOrigin);
            }
            continue;
        }

        if (tok && textMap) {
            DrawTokenSelectionAndSearchHighlights(canvas, *tok, textMap->plainText, overlay, theme, docScreenOrigin);
        }

        const bool isLink = (atom.kind == InlineKind::Link);
        const bool wordHovered = isLink && tok && tok->rect.Contains(localMousePos);
        const Color drawCol = isLink ? theme.palette.accent : baseColor;

        if (!atom.isWhitespace) {
            canvas.DrawTextStyled(atom.text, {screenRect.x, screenTextY}, atom.fontSize, drawCol, atom.style);
        }
        if (isLink) {
            const float uy = screenRect.Bottom() - 2.0f * s;
            const Color uCol = wordHovered ? theme.palette.accent : theme.palette.accent.WithAlpha(140);
            canvas.DrawLine({screenRect.x, uy}, {screenRect.Right(), uy}, uCol, wordHovered ? 1.5f : 1.0f);
        }
        if (atom.strikethrough) {
            const float sy = screenRect.y + screenRect.h * 0.52f;
            canvas.DrawLine({screenRect.x, sy}, {screenRect.Right(), sy}, drawCol.WithAlpha(180), 1.2f);
        }
        if (tok && textMap) {
            DrawTokenCaretIfPresent(canvas, *tok, textMap->plainText, overlay, theme, docScreenOrigin);
        }
    }
}

} // namespace guikit
