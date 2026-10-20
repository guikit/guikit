#include "guikit/panels/CommandPalette.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

struct CommandPalettePersistState {
    std::string query{};
    int highlightedPos{0};
    float scrollOffset{0.0f};
    Vec2 lastMousePos{-10000.0f, -10000.0f};
    bool wasOpen{false};
};

struct FilteredCommandEntry {
    int origIndex{0};
    size_t labelMatchPos{std::string_view::npos};
    size_t labelMatchLen{0};
};

size_t FindCaseInsensitive(std::string_view haystack, std::string_view needle) noexcept {
    if (needle.empty()) return 0;
    if (needle.size() > haystack.size()) return std::string_view::npos;

    const size_t limit = haystack.size() - needle.size();
    for (size_t i = 0; i <= limit; ++i) {
        bool match = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            const unsigned char c1 = static_cast<unsigned char>(haystack[i + j]);
            const unsigned char c2 = static_cast<unsigned char>(needle[j]);
            if (std::tolower(c1) != std::tolower(c2)) {
                match = false;
                break;
            }
        }
        if (match) {
            return i;
        }
    }
    return std::string_view::npos;
}

std::vector<FilteredCommandEntry> FilterItems(
    std::span<const CommandPaletteItem> items,
    std::string_view query
) {
    std::vector<FilteredCommandEntry> result;
    result.reserve(items.size());

    for (size_t i = 0; i < items.size(); ++i) {
        const auto& it = items[i];
        if (query.empty()) {
            result.push_back(FilteredCommandEntry{
                .origIndex = static_cast<int>(i),
                .labelMatchPos = std::string_view::npos,
                .labelMatchLen = 0
            });
            continue;
        }

        const std::string_view labelSv = it.label.View();
        const std::string_view catSv = it.category.View();
        const std::string_view descSv = it.description.View();

        const size_t labelPos = FindCaseInsensitive(labelSv, query);
        if (labelPos != std::string_view::npos) {
            result.push_back(FilteredCommandEntry{
                .origIndex = static_cast<int>(i),
                .labelMatchPos = labelPos,
                .labelMatchLen = query.size()
            });
            continue;
        }

        if (FindCaseInsensitive(catSv, query) != std::string_view::npos ||
            FindCaseInsensitive(descSv, query) != std::string_view::npos) {
            result.push_back(FilteredCommandEntry{
                .origIndex = static_cast<int>(i),
                .labelMatchPos = std::string_view::npos,
                .labelMatchLen = 0
            });
        }
    }
    return result;
}

} // namespace

CommandPaletteResponse CommandPalette::Run(
    Context& ctx,
    bool& isOpen,
    std::span<const CommandPaletteItem> items,
    const CommandPaletteOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    auto& st = ctx.GetOrCreateState<CommandPalettePersistState>(id);

    CommandPaletteResponse res{};
    res.id = id;
    res.ctx = &ctx;
    res.selectedIndex = -1;

    if (!isOpen) {
        if (st.wasOpen) {
            st.wasOpen = false;
            ctx.InvalidateAll();
        }
        return res;
    }

    const UiSize activeSize = ctx.GetCurrentSize();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
    const UiTheme theme = ctx.GetTheme();
    if (pushedTheme != nullptr) {
        ctx.PopTheme();
    }

    // Ensure no underlying InputField captures text input while the CommandPalette is open
    ctx.ClearFocus();

    if (!st.wasOpen) {
        st.query.clear();
        st.highlightedPos = 0;
        st.scrollOffset = 0.0f;
        st.lastMousePos = ctx.GetMousePos();
        st.wasOpen = true;
        ctx.InvalidateAll();
    }

    // 1. Process text input & editing keys
    bool queryModified = false;

    KeyModifiers bsMods{};
    while (ctx.ConsumeKey(Key::Backspace, bsMods)) {
        if (!st.query.empty()) {
            if (bsMods.ctrl) {
                st.query.clear();
            } else {
                const size_t prev = TextUtil::PrevCharBoundary(st.query, st.query.size());
                st.query.erase(prev);
            }
            queryModified = true;
        }
    }

    const std::vector<std::string> pendingText = ctx.ConsumePendingTextInput();
    for (const std::string& chunk : pendingText) {
        for (char c : chunk) {
            const unsigned char uc = static_cast<unsigned char>(c);
            if (uc >= 32 && uc != 127) {
                st.query.push_back(c);
                queryModified = true;
            }
        }
    }

    if (queryModified) {
        st.highlightedPos = 0;
        st.scrollOffset = 0.0f;
        ctx.InvalidateAll();
    }

    // 2. Filter items against current query
    std::vector<FilteredCommandEntry> filtered = FilterItems(items, st.query);
    const int filteredCount = static_cast<int>(filtered.size());

    if (filteredCount == 0) {
        st.highlightedPos = -1;
        st.scrollOffset = 0.0f;
    } else {
        st.highlightedPos = std::clamp(st.highlightedPos, 0, filteredCount - 1);
    }

    // 3. Compute geometry
    const float s = (theme.scale > 0.0f) ? theme.scale : ((activeScale > 0.0f) ? activeScale : 1.0f);
    const Vec2 logicalVp = ctx.GetLogicalViewport();
    Rect backdropRect(0.0f, 0.0f, logicalVp.x, logicalVp.y);
    if (opts.confineToParent) {
        const Rect parentRect = ctx.GetParentPrevFrameBounds();
        if (parentRect.w > 10.0f && parentRect.h > 10.0f) {
            backdropRect = parentRect;
        }
    }

    const float targetW = theme.Scale(std::max(260.0f, opts.width));
    const float cardW = std::round(std::min(std::max(200.0f * s, backdropRect.w - 24.0f * s), targetW));
    const float searchH = std::round(46.0f * s);
    const float rowH = std::round(40.0f * s);
    const float listPadY = std::round(6.0f * s);
    const uint32_t maxVis = std::max<uint32_t>(1u, opts.maxVisibleItems);
    const uint32_t visRows = (filteredCount > 0)
        ? std::min<uint32_t>(static_cast<uint32_t>(filteredCount), maxVis)
        : 1u;
    const float listViewportH = (filteredCount > 0)
        ? (static_cast<float>(visRows) * rowH + listPadY * 2.0f)
        : std::round(64.0f * s);
    const float footerH = opts.showFooterHints ? std::round(30.0f * s) : 0.0f;
    const float cardH = searchH + 1.0f + listViewportH + (opts.showFooterHints ? (1.0f + footerH) : 0.0f);

    const float cardX = std::round(backdropRect.x + (backdropRect.w - cardW) * 0.5f);
    const float cardY = std::round(backdropRect.y + std::max(20.0f * s, backdropRect.h * 0.14f));
    const Rect cardRect(cardX, cardY, cardW, cardH);
    const Rect searchRect(cardX, cardY, cardW, searchH);
    const Rect listRect(cardX, cardY + searchH + 1.0f, cardW, listViewportH);
    const Rect footerRect(cardX, listRect.Bottom() + 1.0f, cardW, footerH);

    // Search bar interactive sub-rects (Clear button and ESC pill)
    const float escW = std::round(34.0f * s);
    const float escH = std::round(20.0f * s);
    const Rect escRect(
        searchRect.Right() - 12.0f * s - escW,
         std::round(searchRect.y + (searchRect.h - escH) * 0.5f),
        escW,
        escH
    );

    const float clearSz = std::round(22.0f * s);
    const Rect clearRect(
        escRect.x - 6.0f * s - clearSz,
        std::round(searchRect.y + (searchRect.h - clearSz) * 0.5f),
        clearSz,
        clearSz
    );

    const float totalListContentH = (filteredCount > 0) ? (static_cast<float>(filteredCount) * rowH) : 0.0f;
    const float innerListViewportH = std::max(0.0f, listViewportH - listPadY * 2.0f);
    const float maxScroll = std::max(0.0f, totalListContentH - innerListViewportH);

    auto scrollHighlightedIntoView = [&]() {
        if (st.highlightedPos < 0 || filteredCount <= 0) return;
        const float itemTop = static_cast<float>(st.highlightedPos) * rowH;
        const float itemBottom = itemTop + rowH;
        if (itemTop < st.scrollOffset) {
            st.scrollOffset = itemTop;
        } else if (itemBottom > st.scrollOffset + innerListViewportH) {
            st.scrollOffset = itemBottom - innerListViewportH;
        }
        st.scrollOffset = std::clamp(st.scrollOffset, 0.0f, maxScroll);
    };

    // 4. Keyboard navigation & execution
    auto moveHighlight = [&](int delta) {
        if (filteredCount <= 0) return;
        st.highlightedPos = std::clamp(st.highlightedPos + delta, 0, filteredCount - 1);
        scrollHighlightedIntoView();
        ctx.InvalidateAll();
    };

    if (ctx.ConsumeKey(Key::Down)) {
        if (filteredCount > 0) {
            st.highlightedPos = (st.highlightedPos + 1) % filteredCount;
            scrollHighlightedIntoView();
            ctx.InvalidateAll();
        }
    } else if (ctx.ConsumeKey(Key::Up)) {
        if (filteredCount > 0) {
            st.highlightedPos = (st.highlightedPos - 1 + filteredCount) % filteredCount;
            scrollHighlightedIntoView();
            ctx.InvalidateAll();
        }
    } else if (ctx.ConsumeKey(Key::Home)) {
        moveHighlight(-filteredCount);
    } else if (ctx.ConsumeKey(Key::End)) {
        moveHighlight(filteredCount);
    } else if (ctx.ConsumeKey(Key::PageUp)) {
        moveHighlight(-static_cast<int>(maxVis));
    } else if (ctx.ConsumeKey(Key::PageDown)) {
        moveHighlight(static_cast<int>(maxVis));
    }

    if (opts.closeOnEscape && ctx.ConsumeKey(Key::Escape)) {
        isOpen = false;
        st.wasOpen = false;
        ctx.InvalidateAll();
        return res;
    }

    if (ctx.ConsumeKey(Key::Enter)) {
        if (st.highlightedPos >= 0 && st.highlightedPos < filteredCount) {
            const int origIdx = filtered[static_cast<size_t>(st.highlightedPos)].origIndex;
            if (origIdx >= 0 && static_cast<size_t>(origIdx) < items.size() && !items[static_cast<size_t>(origIdx)].disabled) {
                res.selectedIndex = origIdx;
                res.changed = true;
                res.clicked = true;
                isOpen = false;
                st.wasOpen = false;
                ctx.InvalidateAll();
                return res;
            }
        }
    }

    // 5. Mouse wheel & hit testing
    const Vec2 mouse = ctx.GetMousePos();
    const bool mouseMoved = (std::abs(mouse.x - st.lastMousePos.x) > 0.5f || std::abs(mouse.y - st.lastMousePos.y) > 0.5f);
    st.lastMousePos = mouse;

    const Vec2 wheel = ctx.GetMouseWheel();
    if (wheel.y != 0.0f && backdropRect.Contains(mouse)) {
        st.scrollOffset = std::clamp(st.scrollOffset - wheel.y * rowH * 1.25f, 0.0f, maxScroll);
        ctx.ConsumeMouseWheel();
        ctx.InvalidateAll();
    }
    st.scrollOffset = std::clamp(st.scrollOffset, 0.0f, maxScroll);

    ctx.BeginOverlayHitCheck(id);

    const bool clearBtnHovered = !st.query.empty() && clearRect.Contains(mouse);
    const bool escPillHovered = opts.closeOnEscape && escRect.Contains(mouse);

    int hoveredFilteredPos = -1;
    if (filteredCount > 0 && listRect.Contains(mouse)) {
        const float relY = (mouse.y - (listRect.y + listPadY)) + st.scrollOffset;
        if (relY >= 0.0f) {
            const int idx = static_cast<int>(relY / rowH);
            if (idx >= 0 && idx < filteredCount) {
                hoveredFilteredPos = idx;
                if (mouseMoved && st.highlightedPos != idx) {
                    st.highlightedPos = idx;
                    ctx.InvalidateAll();
                }
            }
        }
    }

    if (ctx.IsMouseClicked()) {
        if (cardRect.Contains(mouse)) {
            if (clearBtnHovered) {
                st.query.clear();
                st.highlightedPos = 0;
                st.scrollOffset = 0.0f;
                ctx.ConsumeMouseClick();
                ctx.InvalidateAll();
                filtered = FilterItems(items, st.query);
            } else if (escPillHovered) {
                isOpen = false;
                st.wasOpen = false;
                ctx.ConsumeMouseClick();
                ctx.InvalidateAll();
                ctx.EndOverlayHitCheck();
                return res;
            } else if (hoveredFilteredPos >= 0 && hoveredFilteredPos < static_cast<int>(filtered.size())) {
                const int origIdx = filtered[static_cast<size_t>(hoveredFilteredPos)].origIndex;
                if (origIdx >= 0 && static_cast<size_t>(origIdx) < items.size() && !items[static_cast<size_t>(origIdx)].disabled) {
                    res.selectedIndex = origIdx;
                    res.changed = true;
                    res.clicked = true;
                    isOpen = false;
                    st.wasOpen = false;
                    ctx.ConsumeMouseClick();
                    ctx.InvalidateAll();
                    ctx.EndOverlayHitCheck();
                    return res;
                }
                ctx.ConsumeMouseClick();
            } else {
                ctx.ConsumeMouseClick();
            }
        } else if (opts.closeOnBackdropClick && backdropRect.Contains(mouse)) {
            isOpen = false;
            st.wasOpen = false;
            ctx.ConsumeMouseClick();
            ctx.InvalidateAll();
            ctx.EndOverlayHitCheck();
            return res;
        }
    }

    ctx.EndOverlayHitCheck();

    // Block underlying widgets from receiving hover/clicks inside backdropRect
    ctx.RegisterOverlayHitRegion(id, backdropRect);
    ctx.RegisterOverlayHitBuilder([&ctx, id, backdropRect]() {
        ctx.RegisterOverlayHitRegion(id, backdropRect);
    });

    res.rect = cardRect;
    res.hovered = cardRect.Contains(mouse);

    // 6. Snapshot data for deferred overlay rendering
    struct DrawRowItem {
        std::string label;
        std::string category;
        std::string description;
        std::string shortcut;
        Icon icon;
        bool disabled{false};
        bool highlighted{false};
        size_t matchPos{std::string_view::npos};
        size_t matchLen{0};
        Rect rowRect{};
    };

    std::vector<DrawRowItem> drawRows;
    drawRows.reserve(filtered.size());
    for (size_t i = 0; i < filtered.size(); ++i) {
        const auto& fe = filtered[i];
        const auto& src = items[static_cast<size_t>(fe.origIndex)];
        const float rowY = listRect.y + listPadY + static_cast<float>(i) * rowH - st.scrollOffset;
        if (rowY + rowH < listRect.y || rowY > listRect.Bottom()) {
            continue;
        }
        drawRows.push_back(DrawRowItem{
            .label = std::string(src.label.View()),
            .category = std::string(src.category.View()),
            .description = std::string(src.description.View()),
            .shortcut = std::string(src.shortcut.View()),
            .icon = src.icon,
            .disabled = src.disabled,
            .highlighted = (static_cast<int>(i) == st.highlightedPos),
            .matchPos = fe.labelMatchPos,
            .matchLen = fe.labelMatchLen,
            .rowRect = Rect(cardRect.x + 6.0f * s, std::round(rowY), cardRect.w - 12.0f * s, rowH)
        });
    }

    const std::string queryCopy = st.query;
    const std::string placeholderCopy = std::string(opts.placeholder.View());
    const float scrollOffsetCopy = st.scrollOffset;
    const bool showFooter = opts.showFooterHints;
    const float backdropOpacity = opts.backdropOpacity;
    const StateColor bgOpt = opts.background;
    const Border borderOpt = opts.border;
    const bool cursorVis = ctx.IsCursorVisible();
    const size_t totalFiltered = filtered.size();

    ctx.RegisterOverlay([
        backdropRect,
        cardRect,
        searchRect,
        listRect,
        footerRect,
        escRect,
        clearRect,
        clearBtnHovered,
        escPillHovered,
        queryCopy,
        placeholderCopy,
        drawRows = std::move(drawRows),
        totalFiltered,
        totalListContentH,
        innerListViewportH,
        scrollOffsetCopy,
        maxScroll,
        showFooter,
        backdropOpacity,
        bgOpt,
        borderOpt,
        cursorVis,
        activeSize,
        s,
        theme
    ](ICanvas& c) {
        const auto& pal = theme.palette;
        const Color primaryCol = theme.GetVariant(UiVariant::Primary).bg;

        // 1. Dimmed backdrop
        const float alpha = std::clamp(backdropOpacity, 0.0f, 0.95f);
        if (alpha > 0.001f && backdropRect.w > 0.0f && backdropRect.h > 0.0f) {
            c.DrawRect(backdropRect, Color(0, 0, 0, static_cast<uint8_t>(std::round(alpha * 255.0f))));
        }

        // 2. Multi-layer elevated shadow & surface card
        const float radius = borderOpt.radius.has_value()
            ? theme.Scale(*borderOpt.radius)
            : (10.0f * s);
        const float borderW = (borderOpt.width > 0.0f) ? theme.Scale(borderOpt.width) : 1.0f;

        c.DrawRoundRect(
            Rect(cardRect.x - 2.0f * s, cardRect.y + 8.0f * s, cardRect.w + 4.0f * s, cardRect.h + 10.0f * s),
            radius + 4.0f * s,
            Color(0, 0, 0, 65),
            Color::Transparent(),
            0.0f
        );
        c.DrawRoundRect(
            Rect(cardRect.x, cardRect.y + 3.0f * s, cardRect.w, cardRect.h + 2.0f * s),
            radius + 1.0f * s,
            Color(0, 0, 0, 85),
            Color::Transparent(),
            0.0f
        );

        const Color defaultSurface = (theme.mode == ThemeMode::Dark)
            ? Color(19, 26, 41, 254)
            : Color(255, 255, 255, 254);
        const Color surfaceBg = bgOpt.HasAny()
            ? bgOpt.Resolve(false, false, false, defaultSurface)
            : defaultSurface;
        const Color borderCol = borderOpt.color.HasAny()
            ? borderOpt.color.Resolve(false, false, false, pal.borderHover)
            : pal.borderHover;

        c.DrawRoundRect(cardRect, radius, surfaceBg, borderCol, borderW);

        // 3. Top Search Bar
        const float searchIconSz = std::round(16.0f * s);
        const Rect searchIconRect(
            searchRect.x + 14.0f * s,
            std::round(searchRect.y + (searchRect.h - searchIconSz) * 0.5f),
            searchIconSz,
            searchIconSz
        );
        Icon::Search().Draw(c, searchIconRect, primaryCol);

        const float searchFont = theme.ResolveFontSize(UiText{}, TextRole::BodyMedium, activeSize);
        const float textStartX = searchIconRect.Right() + 10.0f * s;
        const float textEndX = (!queryCopy.empty() ? clearRect.x : escRect.x) - 8.0f * s;
        const Rect queryTextRect(textStartX, searchRect.y, std::max(10.0f, textEndX - textStartX), searchRect.h);

        c.PushClip(queryTextRect);
        if (queryCopy.empty()) {
            c.DrawTextBox(placeholderCopy, queryTextRect, searchFont, pal.textMuted, 0.0f, 0.5f);
            if (cursorVis) {
                const float caretH = std::round(searchFont * 1.15f);
                const Rect caretRect(textStartX, std::round(searchRect.Center().y - caretH * 0.5f), std::max(1.5f, std::round(1.5f * s)), caretH);
                c.DrawRect(caretRect, primaryCol);
            }
        } else {
            const float qw = c.MeasureText(queryCopy, searchFont).x;
            const float drawX = (qw > queryTextRect.w - 4.0f * s)
                ? (queryTextRect.Right() - qw - 4.0f * s)
                : textStartX;
            c.DrawTextBox(
                queryCopy,
                Rect(drawX, searchRect.y, qw + 8.0f * s, searchRect.h),
                searchFont,
                pal.textPrimary,
                0.0f,
                0.5f
            );
            if (cursorVis) {
                const float caretH = std::round(searchFont * 1.15f);
                const Rect caretRect(
                    std::round(drawX + qw + 1.0f * s),
                    std::round(searchRect.Center().y - caretH * 0.5f),
                    std::max(1.5f, std::round(1.5f * s)),
                    caretH
                );
                c.DrawRect(caretRect, primaryCol);
            }
        }
        c.PopClip();

        // Clear 'x' button when query is non-empty
        if (!queryCopy.empty()) {
            if (clearBtnHovered) {
                c.DrawRoundRect(clearRect, 5.0f * s, pal.surfaceHover, Color::Transparent(), 0.0f);
            }
            const float xSz = std::round(12.0f * s);
            const Rect xBox(
                std::round(clearRect.x + (clearRect.w - xSz) * 0.5f),
                std::round(clearRect.y + (clearRect.h - xSz) * 0.5f),
                xSz,
                xSz
            );
            Icon::Close().Draw(c, xBox, clearBtnHovered ? pal.textPrimary : pal.textMuted);
        }

        // [ESC] keycap pill on the right of search bar
        const float badgeFont = theme.ResolveFontSize(UiText{}, TextRole::LabelSmall, activeSize);
        const Color keycapBg = escPillHovered ? pal.surfaceActive : pal.surfaceHover;
        c.DrawRoundRect(escRect, 4.0f * s, keycapBg, pal.borderSubtle, 1.0f);
        c.DrawTextBox("ESC", escRect, badgeFont, pal.textMuted, 0.5f, 0.5f);

        // Divider line below search bar
        c.DrawRect(
            Rect(cardRect.x + 1.0f, searchRect.Bottom(), std::max(0.0f, cardRect.w - 2.0f), 1.0f),
            pal.borderSubtle
        );

        // 4. Filtered Command Rows (or empty state)
        c.PushClip(listRect);
        const float labelFont = theme.ResolveFontSize(UiText{}, TextRole::LabelLarge, activeSize);
        const float descFont = theme.ResolveFontSize(UiText{}, TextRole::BodySmall, activeSize);

        if (totalFiltered == 0) {
            c.DrawTextBox(
                "No matching commands",
                listRect,
                labelFont,
                pal.textMuted,
                0.5f,
                0.5f
            );
        } else {
            for (const auto& row : drawRows) {
                const Rect& rr = row.rowRect;

                if (row.highlighted) {
                    c.DrawRoundRect(
                        rr,
                        6.0f * s,
                        primaryCol.WithAlphaF(0.16f),
                        Color::Transparent(),
                        0.0f
                    );
                    // Left 3px accent bar in primary
                    const float barW = std::max(2.0f, std::round(3.0f * s));
                    const float barH = std::max(8.0f, rr.h - 10.0f * s);
                    c.DrawRoundRect(
                        Rect(rr.x + 2.0f * s, std::round(rr.y + (rr.h - barH) * 0.5f), barW, barH),
                        barW * 0.5f,
                        primaryCol,
                        Color::Transparent(),
                        0.0f
                    );
                }

                Color fg = row.disabled ? pal.textDisabled : pal.textPrimary;
                Color mutedFg = row.disabled ? pal.textDisabled : pal.textMuted;

                float curX = rr.x + 12.0f * s;
                float rightEdge = rr.Right() - 10.0f * s;

                // Right-aligned [shortcut] keycap badge
                if (!row.shortcut.empty()) {
                    const float sw = c.MeasureText(row.shortcut, badgeFont).x;
                    const float pillW = std::round(sw + 12.0f * s);
                    const float pillH = std::round(20.0f * s);
                    const Rect pillRect(
                        rightEdge - pillW,
                        std::round(rr.y + (rr.h - pillH) * 0.5f),
                        pillW,
                        pillH
                    );
                    c.DrawRoundRect(
                        pillRect,
                        4.0f * s,
                        row.highlighted ? pal.surfaceActive.WithAlphaF(0.85f) : pal.surfaceHover.WithAlphaF(0.75f),
                        pal.borderSubtle,
                        1.0f
                    );
                    c.DrawTextBox(row.shortcut, pillRect, badgeFont, mutedFg, 0.5f, 0.5f);
                    rightEdge = pillRect.x - 10.0f * s;
                }

                // Leading icon
                const float icSz = std::round(15.0f * s);
                const Rect icRect(
                    curX,
                    std::round(rr.y + (rr.h - icSz) * 0.5f),
                    icSz,
                    icSz
                );
                if (row.icon.IsValid()) {
                    row.icon.Draw(c, icRect, row.highlighted && !row.disabled ? primaryCol : mutedFg);
                } else {
                    Icon::ChevronRight().Draw(c, icRect, mutedFg.WithAlphaF(0.6f));
                }
                curX += icSz + 10.0f * s;

                // Optional [category] pill prefix
                if (!row.category.empty()) {
                    const float catW = c.MeasureText(row.category, badgeFont).x;
                    const float catPillW = std::round(catW + 10.0f * s);
                    const float catPillH = std::round(18.0f * s);
                    const Rect catRect(
                        curX,
                        std::round(rr.y + (rr.h - catPillH) * 0.5f),
                        catPillW,
                        catPillH
                    );
                    c.DrawRoundRect(
                        catRect,
                        4.0f * s,
                        primaryCol.WithAlphaF(row.highlighted ? 0.22f : 0.12f),
                        Color::Transparent(),
                        0.0f
                    );
                    c.DrawTextBox(
                        row.category,
                        catRect,
                        badgeFont,
                        row.disabled ? pal.textDisabled : primaryCol,
                        0.5f,
                        0.5f
                    );
                    curX += catPillW + 8.0f * s;
                }

                // Label with matched substring highlight
                const float availTextW = std::max(0.0f, rightEdge - curX);
                if (availTextW > 4.0f) {
                    const std::string_view labelSv = row.label;
                    const float fullLabelW = c.MeasureText(labelSv, labelFont).x;

                    if (row.matchPos != std::string_view::npos &&
                        row.matchLen > 0 &&
                        row.matchPos + row.matchLen <= labelSv.size()) {
                        const std::string_view prefix = labelSv.substr(0, row.matchPos);
                        const std::string_view matched = labelSv.substr(row.matchPos, row.matchLen);
                        const std::string_view suffix = labelSv.substr(row.matchPos + row.matchLen);

                        const float prefixW = prefix.empty() ? 0.0f : c.MeasureText(prefix, labelFont).x;
                        const float matchW = c.MeasureText(matched, labelFont).x;
                        const float hlH = std::round(labelFont * 1.32f);
                        const Rect hlRect(
                            std::round(curX + prefixW - 1.0f * s),
                            std::round(rr.y + (rr.h - hlH) * 0.5f),
                            std::round(matchW + 2.0f * s),
                            hlH
                        );
                        c.DrawRoundRect(hlRect, 3.0f * s, primaryCol.WithAlphaF(0.28f), Color::Transparent(), 0.0f);

                        if (!prefix.empty()) {
                            c.DrawTextBox(prefix, Rect(curX, rr.y, prefixW + 2.0f, rr.h), labelFont, fg, 0.0f, 0.5f);
                        }
                        const float lineH = c.GetLineHeight(labelFont);
                        const float textY = rr.y + (rr.h - lineH) * 0.5f;
                        c.DrawTextStyled(
                            matched,
                            Vec2(curX + prefixW, textY),
                            labelFont,
                            row.disabled ? fg : primaryCol,
                            FontStyle::Bold
                        );
                        if (!suffix.empty()) {
                            const float suffixX = curX + prefixW + matchW;
                            c.DrawTextBox(
                                suffix,
                                Rect(suffixX, rr.y, std::max(0.0f, rightEdge - suffixX), rr.h),
                                labelFont,
                                fg,
                                0.0f,
                                0.5f
                            );
                        }
                    } else {
                        c.DrawTextBox(
                            labelSv,
                            Rect(curX, rr.y, availTextW, rr.h),
                            labelFont,
                            fg,
                            0.0f,
                            0.5f
                        );
                    }

                    curX += fullLabelW + 10.0f * s;

                    // Optional secondary description
                    if (!row.description.empty() && rightEdge - curX > 24.0f * s) {
                        const std::string truncDesc = TextUtil::TruncateWithEllipsis(
                            row.description,
                            rightEdge - curX,
                            descFont,
                            &c
                        );
                        if (!truncDesc.empty()) {
                            c.DrawTextBox(
                                truncDesc,
                                Rect(curX, rr.y, rightEdge - curX, rr.h),
                                descFont,
                                mutedFg,
                                0.0f,
                                0.5f
                            );
                        }
                    }
                }
            }

            // Subtle scrollbar thumb when list overflows
            if (maxScroll > 0.5f && totalListContentH > 0.0f) {
                const float trackH = listRect.h - 8.0f * s;
                const float thumbH = std::max(18.0f * s, trackH * (innerListViewportH / totalListContentH));
                const float thumbY = listRect.y + 4.0f * s + (trackH - thumbH) * (scrollOffsetCopy / maxScroll);
                c.DrawRoundRect(
                    Rect(cardRect.Right() - 5.0f * s, std::round(thumbY), 3.0f * s, std::round(thumbH)),
                    1.5f * s,
                    pal.textMuted.WithAlphaF(0.45f),
                    Color::Transparent(),
                    0.0f
                );
            }
        }
        c.PopClip();

        // 5. Optional Footer Hints Bar
        if (showFooter && footerRect.h > 0.0f) {
            c.DrawRect(
                Rect(cardRect.x + 1.0f, listRect.Bottom(), std::max(0.0f, cardRect.w - 2.0f), 1.0f),
                pal.borderSubtle
            );

            const Rect footerInner(
                footerRect.x + 14.0f * s,
                footerRect.y,
                std::max(0.0f, footerRect.w - 28.0f * s),
                footerRect.h
            );
            c.DrawTextBox(
                "↑↓ Navigate   ↵ Execute   Esc Close",
                footerInner,
                badgeFont,
                pal.textMuted,
                0.0f,
                0.5f
            );

            const UiString countLabel = (totalFiltered == 1)
                ? UiString("1 command")
                : UiString::FormatNew("{} commands", totalFiltered);
            c.DrawTextBox(
                countLabel.View(),
                footerInner,
                badgeFont,
                pal.textMuted,
                1.0f,
                0.5f
            );
        }
    });

    return res;
}

} // namespace guikit
