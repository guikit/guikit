#include "text_area_internal.hpp"

namespace guikit {

namespace {

[[nodiscard]] bool IsHexDigitAscii(char c) noexcept {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

[[nodiscard]] bool IsMarkupNameChar(char c) noexcept {
    return detail::ta_impl::IsIdentChar(c) || c == '-' || c == ':' || c == '.';
}

/// Checks whether `ls[pos]` (where `ls[pos] == '\''`) starts a Rust char literal ('x', '\n', '\u{...}')
/// rather than a Rust lifetime ('a, 'static, '_). If true, sets `outEnd` past the closing `'`.
[[nodiscard]] bool TryMatchRustCharLiteral(keel::StringRef ls, keel::usize pos, keel::usize& outEnd) noexcept {
    const keel::usize n = ls.size();
    if (pos + 2u >= n || ls[pos] != '\'') return false;
    if (ls[pos + 1u] == '\\') {
        keel::usize k = pos + 2u;
        if (k < n) ++k; // consume escaped char
        while (k < n && (k - pos) <= 14u && ls[k] != '\'') {
            ++k;
        }
        if (k < n && ls[k] == '\'') {
            outEnd = k + 1u;
            return true;
        }
        return false;
    }
    if (ls[pos + 1u] != '\'' && ls[pos + 2u] == '\'') {
        outEnd = pos + 3u;
        return true;
    }
    return false;
}

[[nodiscard]] bool BinarySearchSortedTokens(keel::ArrayRef<keel::StringRef> list, keel::StringRef key) noexcept {
    keel::usize lo = 0u;
    keel::usize hi = list.size();
    while (lo < hi) {
        const keel::usize mid = lo + (hi - lo) / 2u;
        const keel::StringRef cur = list[mid];
        if (cur < key) {
            lo = mid + 1u;
        } else if (key < cur) {
            hi = mid;
        } else {
            return true;
        }
    }
    return false;
}

void AdvanceBlockCommentStateOnLine(
    keel::StringRef ls,
    const SyntaxConfig& cfg,
    bool& inBlockComment
) noexcept {
    if (ls.empty() || ls.size() > detail::ta_impl::kMaxSyntaxLineBytes) return;
    const keel::StringRef bStartTok = cfg.blockCommentStart;
    const keel::StringRef bEndTok = cfg.blockCommentEnd;
    const keel::StringRef lCmt = cfg.lineComment;
    if (bStartTok.empty() || bEndTok.empty()) return;

    keel::usize pos = 0u;
    const keel::usize n = ls.size();
    while (pos < n) {
        if (!inBlockComment) {
            const char c = ls[pos];
            // Check blockCommentStart BEFORE string/lineComment (handles Python """ and Lua --[[ and XML <!--)
            if (pos + bStartTok.size() <= n && ls.substr(pos, bStartTok.size()) == bStartTok) {
                inBlockComment = true;
                pos += bStartTok.size();
                continue;
            }
            if (!lCmt.empty() && pos + lCmt.size() <= n && ls.substr(pos, lCmt.size()) == lCmt) {
                break;
            }
            // Skip quoted string literals so "/*" or "<!--" inside strings does not open a block comment
            const bool isQuote = cfg.highlightStrings && (
                c == '"' ||
                (c == '\'' && cfg.singleQuoteStrings) ||
                (c == '`' && cfg.backtickStrings)
            );
            if (isQuote && !cfg.highlightMarkupTags) {
                if (c == '\'' && cfg.rustLifetimes) {
                    keel::usize charEnd = 0u;
                    if (TryMatchRustCharLiteral(ls, pos, charEnd)) {
                        pos = charEnd;
                        continue;
                    }
                    ++pos;
                    continue;
                }
                if (c == '\'' && cfg.transposeApostrophe && pos > 0u) {
                    const char prev = ls[pos - 1u];
                    if (detail::ta_impl::IsIdentChar(prev) || prev == ')' || prev == ']' || prev == '}' || prev == '.') {
                        ++pos;
                        continue;
                    }
                }
                const char q = c;
                ++pos;
                while (pos < n) {
                    if (q != '`' && ls[pos] == '\\' && pos + 1u < n) {
                        pos += 2u;
                        continue;
                    }
                    if (ls[pos] == q) {
                        ++pos;
                        break;
                    }
                    ++pos;
                }
                continue;
            }
            ++pos;
        } else {
            const keel::usize bEnd = ls.find(bEndTok, pos);
            if (bEnd == keel::StringRef::npos) {
                break;
            }
            inBlockComment = false;
            pos = bEnd + bEndTok.size();
        }
    }
}

void HighlightMarkupLine(
    keel::StringRef lineText,
    keel::usize startPos,
    const SyntaxConfig& config,
    keel::Array<TextSpan>& outSpans,
    const SyntaxPalette& pal,
    bool* inOutBlockComment
) {
    const keel::usize len = lineText.size();
    keel::usize i = startPos;

    while (i < len) {
        // 1. Markup comment <!-- ... -->
        if (!config.blockCommentStart.empty() &&
            i + config.blockCommentStart.size() <= len &&
            lineText.substr(i, config.blockCommentStart.size()) == config.blockCommentStart) {
            const keel::usize start = i;
            i += config.blockCommentStart.size();
            if (!config.blockCommentEnd.empty()) {
                const keel::usize endPos = lineText.find(config.blockCommentEnd, i);
                if (endPos != keel::StringRef::npos) {
                    i = endPos + config.blockCommentEnd.size();
                } else {
                    i = len;
                    if (inOutBlockComment) *inOutBlockComment = true;
                }
            } else {
                i = len;
            }
            outSpans.push_back({keel::as<keel::u32>(start), keel::as<keel::u32>(i - start), pal.comment});
            continue;
        }

        // 2. CDATA section <![CDATA[ ... ]]>
        if (i + 9u <= len && lineText.substr(i, 9u) == "<![CDATA[") {
            outSpans.push_back({keel::as<keel::u32>(i), 9u, pal.preprocessor});
            i += 9u;
            const keel::usize endCdata = lineText.find("]]>", i);
            if (endCdata != keel::StringRef::npos) {
                if (endCdata > i) {
                    outSpans.push_back({keel::as<keel::u32>(i), keel::as<keel::u32>(endCdata - i), pal.string});
                }
                outSpans.push_back({keel::as<keel::u32>(endCdata), 3u, pal.preprocessor});
                i = endCdata + 3u;
            } else {
                if (len > i) {
                    outSpans.push_back({keel::as<keel::u32>(i), keel::as<keel::u32>(len - i), pal.string});
                }
                i = len;
            }
            continue;
        }

        // 3. Tag: <tag ...>, </tag>, <?xml ...?>, <!DOCTYPE ...>
        if (lineText[i] == '<') {
            const keel::usize openStart = i++;
            bool isMeta = false;
            if (i < len && (lineText[i] == '/' || lineText[i] == '?' || lineText[i] == '!')) {
                isMeta = (lineText[i] == '?' || lineText[i] == '!');
                ++i;
            }
            outSpans.push_back({keel::as<keel::u32>(openStart), keel::as<keel::u32>(i - openStart), pal.punctuation});

            // Tag name
            if (i < len && detail::ta_impl::IsIdentStart(lineText[i])) {
                const keel::usize nameStart = i++;
                while (i < len && IsMarkupNameChar(lineText[i])) ++i;
                outSpans.push_back({
                    keel::as<keel::u32>(nameStart),
                    keel::as<keel::u32>(i - nameStart),
                    isMeta ? pal.preprocessor : pal.keyword
                });
            }

            // Attributes & values until '>' / '/>' / '?>'
            while (i < len) {
                const char c = lineText[i];
                if (c == ' ' || c == '\t') {
                    const keel::usize ws = i++;
                    while (i < len && (lineText[i] == ' ' || lineText[i] == '\t')) ++i;
                    outSpans.push_back({keel::as<keel::u32>(ws), keel::as<keel::u32>(i - ws), pal.identifier});
                    continue;
                }
                if (c == '>') {
                    outSpans.push_back({keel::as<keel::u32>(i), 1u, pal.punctuation});
                    ++i;
                    break;
                }
                if ((c == '/' || c == '?') && i + 1u < len && lineText[i + 1u] == '>') {
                    outSpans.push_back({keel::as<keel::u32>(i), 2u, pal.punctuation});
                    i += 2u;
                    break;
                }
                if (c == '"' || c == '\'') {
                    const char q = c;
                    const keel::usize sStart = i++;
                    while (i < len && lineText[i] != q) ++i;
                    if (i < len) ++i;
                    outSpans.push_back({keel::as<keel::u32>(sStart), keel::as<keel::u32>(i - sStart), pal.string});
                    continue;
                }
                if (detail::ta_impl::IsIdentStart(c)) {
                    const keel::usize aStart = i++;
                    while (i < len && IsMarkupNameChar(lineText[i])) ++i;
                    outSpans.push_back({keel::as<keel::u32>(aStart), keel::as<keel::u32>(i - aStart), pal.type});
                    continue;
                }
                outSpans.push_back({keel::as<keel::u32>(i), 1u, pal.punctuation});
                ++i;
            }
            continue;
        }

        // 4. Entity reference (&amp;, &#x20;, &#160;)
        if (lineText[i] == '&') {
            keel::usize k = i + 1u;
            if (k < len && lineText[k] == '#') ++k;
            const keel::usize bodyStart = k;
            while (k < len && (k - i) <= 16u && keel::is_ascii_alnum(lineText[k])) ++k;
            if (k > bodyStart && k < len && lineText[k] == ';') {
                ++k;
                outSpans.push_back({keel::as<keel::u32>(i), keel::as<keel::u32>(k - i), pal.number});
                i = k;
                continue;
            }
            outSpans.push_back({keel::as<keel::u32>(i), 1u, pal.identifier});
            ++i;
            continue;
        }

        // 5. Plain text between markup tokens
        const keel::usize txtStart = i++;
        while (i < len && lineText[i] != '<' && lineText[i] != '&') ++i;
        outSpans.push_back({keel::as<keel::u32>(txtStart), keel::as<keel::u32>(i - txtStart), pal.identifier});
    }
}

} // namespace

namespace detail::ta_impl {

bool ContainsToken(keel::ArrayRef<keel::StringRef> list, keel::StringRef token, bool caseInsensitive) noexcept {
    if (list.empty() || token.empty()) return false;
    if (!caseInsensitive) {
        if (IsBuiltInSortedTokenList(list)) {
            return BinarySearchSortedTokens(list, token);
        }
        const keel::usize tokLen = token.size();
        for (keel::StringRef item : list) {
            if (item.size() == tokLen && item == token) return true;
        }
        return false;
    }

    if (token.size() > 64u) return false;
    keel::SmallString<64> lower;
    lower.reserve(token.size());
    for (char c : token) {
        lower.push_back(ToLowerAscii(c));
    }
    const keel::StringRef lowerRef = lower.as_ref();
    if (IsBuiltInSortedTokenList(list)) {
        return BinarySearchSortedTokens(list, lowerRef);
    }
    const keel::usize tokLen = lowerRef.size();
    for (keel::StringRef item : list) {
        if (item.size() != tokLen) continue;
        bool eq = true;
        for (keel::usize k = 0u; k < tokLen; ++k) {
            if (ToLowerAscii(item[k]) != lowerRef[k]) {
                eq = false;
                break;
            }
        }
        if (eq) return true;
    }
    return false;
}

void InvalidateLexerCheckpointsFromLine(TextAreaState& state, keel::usize dirtyLine) noexcept {
    // Checkpoint index `cp` stores state entering line `cp * kLexerCheckpointStride`.
    // Any edit on `dirtyLine` can change the state entering `(dirtyLine / stride + 1) * stride` and beyond.
    const keel::usize keepValid = (dirtyLine / kLexerCheckpointStride) + 1u;
    if (state.lexerValidCheckpoints > keepValid) {
        state.lexerValidCheckpoints = keepValid;
    }
}

bool ResolveBlockCommentStateAtLine(
    const TextSourceView& source,
    const SyntaxConfig& cfg,
    keel::usize targetLine,
    keel::u64 docKey,
    TextAreaState* state
) {
    if (!cfg.enabled || cfg.blockCommentStart.empty() || cfg.blockCommentEnd.empty() || targetLine == 0u) {
        return false;
    }

    const keel::usize totalLines = source.LineCount();
    if (targetLine >= totalLines) {
        targetLine = totalLines - 1u;
    }

    // Fallback (stateless Render without TextAreaState): scan up to 250 lines back.
    if (!state) {
        bool inBlock = false;
        const keel::usize scanStart = (targetLine > 250u) ? (targetLine - 250u) : 0u;
        keel::String scratch;
        for (keel::usize li = scanStart; li < targetLine; ++li) {
            const TextLineRange lr = source.LineByteRange(li);
            if (lr.contentBytes == 0u || lr.contentBytes > kMaxSyntaxLineBytes) continue;
            AdvanceBlockCommentStateOnLine(source.ReadSliceRef(lr.startByte, lr.contentBytes, scratch), cfg, inBlock);
        }
        return inBlock;
    }

    const keel::u64 cfgKey = detail::HashArgs(
        cfg.blockCommentStart,
        cfg.blockCommentEnd,
        cfg.lineComment,
        cfg.highlightStrings,
        cfg.transposeApostrophe,
        cfg.singleQuoteStrings,
        cfg.backtickStrings,
        cfg.rustLifetimes,
        cfg.highlightMarkupTags
    );
    const keel::usize totalCheckpoints = (totalLines + kLexerCheckpointStride - 1u) / kLexerCheckpointStride;

    if (state->cachedLexerConfigKey != cfgKey || state->lexerCheckpoints.empty()) {
        state->cachedLexerConfigKey = cfgKey;
        state->cachedLexerDocKey = docKey;
        state->lexerCheckpoints.resize(keel::max<keel::usize>(1u, totalCheckpoints));
        state->lexerCheckpoints[0] = 0u; // Line 0 always starts outside block comments
        state->lexerValidCheckpoints = 1u;
    } else if (state->lexerCheckpoints.size() < totalCheckpoints) {
        state->lexerCheckpoints.resize(totalCheckpoints);
    }

    if (state->cachedLexerDocKey != docKey) {
        // For flat buffers (non-TextDocument), invalidate back to checkpoint 1 on external doc change.
        // For TextDocument, incremental invalidation is already done via delta replay in RunTextAreaCore!
        if (!source.IsDocument()) {
            state->lexerValidCheckpoints = 1u;
        }
        state->cachedLexerDocKey = docKey;
    }

    const keel::usize reqCp = targetLine / kLexerCheckpointStride;
    if (reqCp >= state->lexerCheckpoints.size()) {
        state->lexerCheckpoints.resize(reqCp + 1u);
    }

    // Lazily advance valid checkpoints up to reqCp in a single zero-copy streaming pass!
    if (reqCp >= state->lexerValidCheckpoints) {
        const keel::usize startCp = (state->lexerValidCheckpoints > 0u) ? (state->lexerValidCheckpoints - 1u) : 0u;
        const keel::usize startLine = startCp * kLexerCheckpointStride;
        const keel::usize endLine = reqCp * kLexerCheckpointStride;
        bool inBlock = (state->lexerCheckpoints[startCp] != 0u);

        const keel::usize byteStart = source.LineByteRange(startLine).startByte;
        const keel::usize byteEnd = source.LineByteRange(endLine).startByte;

        keel::String lineAccum;
        keel::usize curLine = startLine;
        bool lineOverflowed = false;

        auto commitLine = [&](keel::StringRef lineView) {
            if (!lineOverflowed) {
                if (!lineView.empty() && lineView.back() == '\r') {
                    lineView.remove_suffix(1u);
                }
                AdvanceBlockCommentStateOnLine(lineView, cfg, inBlock);
            }
            ++curLine;
            if ((curLine % kLexerCheckpointStride) == 0u) {
                const keel::usize cpIdx = curLine / kLexerCheckpointStride;
                if (cpIdx < state->lexerCheckpoints.size()) {
                    state->lexerCheckpoints[cpIdx] = inBlock ? 1u : 0u;
                }
            }
            lineAccum.clear();
            lineOverflowed = false;
        };

        if (byteEnd > byteStart) {
            source.ForEachChunkInRange(byteStart, byteEnd - byteStart, [&](keel::StringRef chunk) -> bool {
                keel::usize pos = 0u;
                const keel::usize n = chunk.size();
                while (pos < n && curLine < endLine) {
                    const keel::usize nl = chunk.find('\n', pos);
                    if (nl == keel::StringRef::npos) {
                        const keel::usize rem = n - pos;
                        if (!lineOverflowed) {
                            if (lineAccum.size() + rem <= kMaxSyntaxLineBytes) {
                                lineAccum.append(chunk.substr(pos, rem));
                            } else {
                                lineOverflowed = true;
                                lineAccum.clear();
                            }
                        }
                        break;
                    }
                    const keel::usize segLen = nl - pos;
                    if (lineAccum.empty() && !lineOverflowed) {
                        // Zero-copy fast path: entire line lies inside this chunk!
                        if (segLen <= kMaxSyntaxLineBytes) {
                            commitLine(chunk.substr(pos, segLen));
                        } else {
                            lineOverflowed = true;
                            commitLine({});
                        }
                    } else {
                        if (!lineOverflowed) {
                            if (lineAccum.size() + segLen <= kMaxSyntaxLineBytes) {
                                lineAccum.append(chunk.substr(pos, segLen));
                            } else {
                                lineOverflowed = true;
                                lineAccum.clear();
                            }
                        }
                        commitLine(lineAccum.as_ref());
                    }
                    pos = nl + 1u;
                }
                return curLine < endLine;
            });
        }
        state->lexerValidCheckpoints = reqCp + 1u;
    }

    // Start from exact checkpoint at reqCp * 256 and scan remaining < 256 lines (< 20 us!).
    bool inBlock = (state->lexerCheckpoints[reqCp] != 0u);
    const keel::usize cpLine = reqCp * kLexerCheckpointStride;
    keel::String tailScratch;
    for (keel::usize li = cpLine; li < targetLine; ++li) {
        const TextLineRange lr = source.LineByteRange(li);
        if (lr.contentBytes == 0u || lr.contentBytes > kMaxSyntaxLineBytes) continue;
        AdvanceBlockCommentStateOnLine(source.ReadSliceRef(lr.startByte, lr.contentBytes, tailScratch), cfg, inBlock);
    }
    return inBlock;
}

} // namespace detail::ta_impl

using namespace detail::ta_impl;

void detail::TextArea::HighlightLine(
    keel::StringRef lineText,
    keel::usize /*lineIndex*/,
    const SyntaxConfig& config,
    keel::Array<TextSpan>& outSpans,
    const UiTheme& theme,
    const SyntaxPalette* paletteOverride,
    bool* inOutBlockComment
) {
    outSpans.clear();
    if (lineText.empty()) return;

    const SyntaxPalette& pal = paletteOverride
        ? *paletteOverride
        : (config.palette.has_value() ? *config.palette : theme.syntax);

    if (!config.enabled) {
        outSpans.push_back({0, keel::as<keel::u32>(lineText.size()), pal.identifier});
        return;
    }

    const keel::usize len = lineText.size();
    keel::usize i = 0;

    // Stateful multiline block comment continuation from previous line
    if (inOutBlockComment && *inOutBlockComment && !config.blockCommentEnd.empty()) {
        const keel::usize endPos = lineText.find(config.blockCommentEnd);
        if (endPos == keel::StringRef::npos) {
            outSpans.push_back({0, keel::as<keel::u32>(len), pal.comment});
            return;
        }
        i = endPos + config.blockCommentEnd.size();
        outSpans.push_back({0, keel::as<keel::u32>(i), pal.comment});
        *inOutBlockComment = false;
    }

    // Markup mode (XML / HTML / SVG / BPMN)
    if (config.highlightMarkupTags) {
        HighlightMarkupLine(lineText, i, config, outSpans, pal, inOutBlockComment);
        return;
    }

    // Skip leading whitespace
    if (i == 0) {
        while (i < len && (lineText[i] == ' ' || lineText[i] == '\t')) {
            ++i;
        }
        if (i > 0) {
            outSpans.push_back({0, keel::as<keel::u32>(i), pal.identifier});
        }
    }

    // Section header check (e.g. "%%" in MATLAB, "// ===" in C++, "// MARK:" in Swift)
    if (!config.sectionPrefix.empty() && i + config.sectionPrefix.size() <= len &&
        lineText.substr(i, config.sectionPrefix.size()) == config.sectionPrefix) {
        outSpans.push_back({keel::as<keel::u32>(i), keel::as<keel::u32>(len - i), pal.preprocessor});
        return;
    }

    // Preprocessor / directive / decorator line check (#include, `define, @Composable, @media)
    if (config.preprocessorPrefix != '\0' && i < len && lineText[i] == config.preprocessorPrefix) {
        keel::usize qPos = keel::StringRef::npos;
        for (keel::usize k = i + 1; k < len; ++k) {
            if (lineText[k] == '"' || (config.preprocessorPrefix == '#' && lineText[k] == '<')) {
                qPos = k;
                break;
            }
        }
        if (qPos != keel::StringRef::npos && config.highlightStrings) {
            outSpans.push_back({keel::as<keel::u32>(i), keel::as<keel::u32>(qPos - i), pal.preprocessor});
            outSpans.push_back({keel::as<keel::u32>(qPos), keel::as<keel::u32>(len - qPos), pal.string});
        } else {
            outSpans.push_back({keel::as<keel::u32>(i), keel::as<keel::u32>(len - i), pal.preprocessor});
        }
        return;
    }

    const bool ci = config.caseInsensitiveKeywords;
    auto isWordChar = [&](char ch) noexcept -> bool {
        return IsIdentChar(ch) || (config.allowHyphenInIdentifiers && ch == '-');
    };

    while (i < len) {
        // 1. Whitespace run
        if (lineText[i] == ' ' || lineText[i] == '\t') {
            const keel::usize start = i;
            while (i < len && (lineText[i] == ' ' || lineText[i] == '\t')) {
                ++i;
            }
            outSpans.push_back({keel::as<keel::u32>(start), keel::as<keel::u32>(i - start), pal.identifier});
            continue;
        }

        // 2. Block comment (checked BEFORE lineComment so "%{" / "--[[" / "#[[" match before "%" / "--" / "#")
        if (!config.blockCommentStart.empty() &&
            i + config.blockCommentStart.size() <= len &&
            lineText.substr(i, config.blockCommentStart.size()) == config.blockCommentStart) {
            const keel::usize start = i;
            i += config.blockCommentStart.size();
            if (!config.blockCommentEnd.empty()) {
                const keel::usize endPos = lineText.find(config.blockCommentEnd, i);
                if (endPos != keel::StringRef::npos) {
                    i = endPos + config.blockCommentEnd.size();
                } else {
                    i = len;
                    if (inOutBlockComment) {
                        *inOutBlockComment = true;
                    }
                }
            } else {
                i = len;
            }
            outSpans.push_back({keel::as<keel::u32>(start), keel::as<keel::u32>(i - start), pal.comment});
            continue;
        }

        // 3. Single-line comment
        if (!config.lineComment.empty() &&
            i + config.lineComment.size() <= len &&
            lineText.substr(i, config.lineComment.size()) == config.lineComment) {
            outSpans.push_back({keel::as<keel::u32>(i), keel::as<keel::u32>(len - i), pal.comment});
            break;
        }

        // 4. String / char / backtick literal & Rust lifetimes
        const bool startsString = config.highlightStrings && (
            lineText[i] == '"' ||
            (lineText[i] == '\'' && config.singleQuoteStrings) ||
            (lineText[i] == '`' && config.backtickStrings)
        );
        if (startsString) {
            if (lineText[i] == '\'' && config.rustLifetimes) {
                keel::usize charEnd = 0u;
                if (TryMatchRustCharLiteral(lineText, i, charEnd)) {
                    outSpans.push_back({keel::as<keel::u32>(i), keel::as<keel::u32>(charEnd - i), pal.string});
                    i = charEnd;
                    continue;
                }
                if (i + 1u < len && IsIdentStart(lineText[i + 1u])) {
                    const keel::usize start = i;
                    i += 2u;
                    while (i < len && IsIdentChar(lineText[i])) ++i;
                    outSpans.push_back({keel::as<keel::u32>(start), keel::as<keel::u32>(i - start), pal.type});
                    continue;
                }
                outSpans.push_back({keel::as<keel::u32>(i), 1u, config.highlightPunctuation ? pal.punctuation : pal.identifier});
                ++i;
                continue;
            }
            if (lineText[i] == '\'' && config.transposeApostrophe && i > 0) {
                const char prev = lineText[i - 1];
                if (IsIdentChar(prev) || prev == ')' || prev == ']' || prev == '}' || prev == '.') {
                    outSpans.push_back({keel::as<keel::u32>(i), 1u, config.highlightPunctuation ? pal.punctuation : pal.identifier});
                    ++i;
                    continue;
                }
            }
            const char quote = lineText[i];
            const keel::usize start = i++;
            while (i < len) {
                if (quote != '`' && lineText[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (lineText[i] == quote) {
                    ++i;
                    break;
                }
                ++i;
            }
            Color strCol = pal.string;
            if (config.highlightJsonKeys && quote == '"') {
                keel::usize look = i;
                while (look < len && (lineText[look] == ' ' || lineText[look] == '\t')) ++look;
                if (look < len && lineText[look] == ':') {
                    strCol = pal.type;
                }
            }
            outSpans.push_back({keel::as<keel::u32>(start), keel::as<keel::u32>(i - start), strCol});
            continue;
        }

        // 5. CSS hex color literal (#rgb, #rrggbb, #rrggbbaa)
        if (config.allowHyphenInIdentifiers && config.highlightNumbers && lineText[i] == '#' && i + 3u < len) {
            keel::usize k = i + 1u;
            while (k < len && IsHexDigitAscii(lineText[k])) ++k;
            const keel::usize hexLen = k - (i + 1u);
            const bool hexBoundary = (k == len) || !isWordChar(lineText[k]);
            if (hexBoundary && (hexLen == 3u || hexLen == 4u || hexLen == 6u || hexLen == 8u)) {
                outSpans.push_back({keel::as<keel::u32>(i), keel::as<keel::u32>(k - i), pal.number});
                i = k;
                continue;
            }
        }

        // 6. Numeric literal (including Verilog sized literals 8'hFF, C++ digit separators 1'000, CSS units 16px)
        if (config.highlightNumbers &&
            (keel::is_ascii_digit(lineText[i]) ||
             (lineText[i] == '.' && i + 1 < len && keel::is_ascii_digit(lineText[i + 1])))) {
            const keel::usize start = i++;
            while (i < len && (keel::is_ascii_alnum(lineText[i]) || lineText[i] == '.' || lineText[i] == '_' ||
                               (!config.transposeApostrophe && lineText[i] == '\''))) {
                ++i;
            }
            outSpans.push_back({keel::as<keel::u32>(start), keel::as<keel::u32>(i - start), pal.number});
            continue;
        }

        // 7. System tasks / $-prefixed builtins (e.g. Verilog $display, $clog2, $finish)
        if (lineText[i] == '$' && !config.builtins.empty() && i + 1u < len && IsIdentStart(lineText[i + 1u])) {
            keel::usize k = i + 2u;
            while (k < len && IsIdentChar(lineText[k])) ++k;
            const keel::StringRef sysTok = lineText.substr(i, k - i);
            if (ContainsToken(config.builtins, sysTok, ci)) {
                outSpans.push_back({keel::as<keel::u32>(i), keel::as<keel::u32>(k - i), pal.function});
                i = k;
                continue;
            }
        }

        // 8. Identifier / keyword / type / builtin / unquoted key / function call (O(log K) on built-in tables!)
        if (IsIdentStart(lineText[i])) {
            const keel::usize start = i++;
            while (i < len && isWordChar(lineText[i])) {
                ++i;
            }
            const keel::StringRef word = lineText.substr(start, i - start);
            // Word-delimited line comment keyword (e.g. "REM ..." in Windows Batch/CMD)
            if (!config.lineCommentKeyword.empty() && word.size() == config.lineCommentKeyword.size()) {
                bool isCmtKw = true;
                for (keel::usize k = 0u; k < word.size(); ++k) {
                    const char a = ci ? ToLowerAscii(word[k]) : word[k];
                    const char b = ci ? ToLowerAscii(config.lineCommentKeyword[k]) : config.lineCommentKeyword[k];
                    if (a != b) { isCmtKw = false; break; }
                }
                if (isCmtKw) {
                    keel::usize prevIdx = start;
                    while (prevIdx > 0u && (lineText[prevIdx - 1u] == ' ' || lineText[prevIdx - 1u] == '\t')) --prevIdx;
                    const bool atStmtStart = (prevIdx == 0u) || lineText[prevIdx - 1u] == '&' || lineText[prevIdx - 1u] == '|' || lineText[prevIdx - 1u] == '(';
                    if (atStmtStart) {
                        outSpans.push_back({keel::as<keel::u32>(start), keel::as<keel::u32>(len - start), pal.comment});
                        break;
                    }
                }
            }
            Color tokenColor = pal.identifier;
            if (ContainsToken(config.keywords, word, ci)) {
                tokenColor = pal.keyword;
            } else if (ContainsToken(config.types, word, ci)) {
                tokenColor = pal.type;
            } else if (ContainsToken(config.builtins, word, ci)) {
                tokenColor = pal.function;
            } else {
                keel::usize look = i;
                while (look < len && (lineText[look] == ' ' || lineText[look] == '\t')) {
                    ++look;
                }
                if (config.highlightUnquotedKeys && look < len && lineText[look] == ':' &&
                    (look + 1u == len || lineText[look + 1u] == ' ' || lineText[look + 1u] == '\t')) {
                    tokenColor = pal.type;
                } else if (config.highlightFunctions && look < len && lineText[look] == '(') {
                    tokenColor = pal.function;
                }
            }
            outSpans.push_back({keel::as<keel::u32>(start), keel::as<keel::u32>(i - start), tokenColor});
            continue;
        }

        // 9. Punctuation / operators
        const keel::usize start = i++;
        while (i < len &&
               lineText[i] != ' ' && lineText[i] != '\t' &&
               lineText[i] != '"' &&
               !(lineText[i] == '\'' && config.singleQuoteStrings) &&
               !(lineText[i] == '`' && config.backtickStrings) &&
               !(lineText[i] == '#' && config.allowHyphenInIdentifiers) &&
               !(lineText[i] == '$' && !config.builtins.empty()) &&
               !IsIdentStart(lineText[i]) &&
               !keel::is_ascii_digit(lineText[i])) {
            if (!config.blockCommentStart.empty() &&
                i + config.blockCommentStart.size() <= len &&
                lineText.substr(i, config.blockCommentStart.size()) == config.blockCommentStart) {
                break;
            }
            if (!config.lineComment.empty() &&
                i + config.lineComment.size() <= len &&
                lineText.substr(i, config.lineComment.size()) == config.lineComment) {
                break;
            }
            ++i;
        }
        outSpans.push_back({
            keel::as<keel::u32>(start),
            keel::as<keel::u32>(i - start),
            config.highlightPunctuation ? pal.punctuation : pal.identifier
        });
    }
}

void detail::TextArea::CppHighlighter(
    keel::StringRef lineText,
    keel::usize lineIndex,
    keel::Array<TextSpan>& outSpans,
    const UiTheme& theme
) {
    HighlightLine(lineText, lineIndex, SyntaxConfig::Cpp(), outSpans, theme, nullptr, nullptr);
}

} // namespace guikit
