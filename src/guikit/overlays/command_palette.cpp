#include "guikit/overlays/command_palette.hpp"
#include "guikit/style/style_painter.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/scrollbar.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/format.h"
#include "keel/math.h"
#include "keel/small_array.h"
#include "keel/static_array.h"
#include "keel/string.h"

namespace guikit {

namespace {

struct CommandPalettePersistState {
    keel::SmallString<128> query{};
    keel::i32 highlightedPos{0};
    keel::f32 scrollOffset{0.0f};
    Vec2 lastMousePos{-10000.0f, -10000.0f};
    bool wasOpen{false};
    bool prevClearHovered{false};
    bool prevEscHovered{false};
    bool prevCursorVis{true};
    ScrollbarState scrollbar{};
};

struct FilteredCommandEntry {
    keel::i32 origIndex{0};
    keel::usize labelMatchPos{keel::kInvalidIndex};
    keel::usize labelMatchLen{0};
};

struct DrawRowItem {
    keel::StringRef label;
    keel::StringRef category;
    keel::StringRef description;
    keel::StringRef shortcut;
    Icon icon;
    bool disabled{false};
    bool highlighted{false};
    keel::usize matchPos{keel::kInvalidIndex};
    keel::usize matchLen{0};
    Rect rowRect{};
};

struct PaletteLayout {
    Rect backdropRect{};
    Rect cardRect{};
    Rect searchRect{};
    Rect listRect{};
    Rect listClipRect{};
    Rect sbViewport{};
    Rect footerRect{};
    Rect escRect{};
    Rect clearRect{};

    keel::f32 searchFont{14.0f};
    keel::f32 labelFont{13.0f};
    keel::f32 descFont{12.0f};
    keel::f32 badgeFont{11.0f};

    keel::f32 rowH{36.0f};
    keel::f32 listPadY{4.0f};
    keel::f32 rowInsetX{6.0f};
    keel::f32 rowPadX{10.0f};
    keel::f32 rowRightPad{16.0f};
    keel::f32 rowRadius{0.0f};
    keel::f32 icSz{16.0f};
    keel::f32 catPillH{18.0f};
    keel::f32 keyPillH{20.0f};
    keel::f32 maxCatPillW{0.0f};

    keel::f32 searchIconPad{12.0f};
    keel::f32 searchIconGap{10.0f};
    keel::f32 textRightEdgeEmpty{0.0f};
    keel::f32 divInset{1.0f};
    keel::f32 footerDivY{0.0f};
    keel::f32 footerPadX{12.0f};

    keel::u32 maxVis{9u};
    bool recessed{false};
    bool showFooter{true};
};

keel::SmallArray<FilteredCommandEntry, 64> FilterItems(
    keel::ArrayRef<CommandPaletteItem> items,
    keel::StringRef query
) {
    keel::SmallArray<FilteredCommandEntry, 64> result;
    result.reserve(items.size());

    for (keel::usize i = 0; i < items.size(); ++i) {
        const auto& it = items[i];
        if (query.empty()) {
            result.push_back(FilteredCommandEntry{
                .origIndex = keel::as<keel::i32>(i),
                .labelMatchPos = keel::kInvalidIndex,
                .labelMatchLen = 0
            });
            continue;
        }

        const keel::StringRef labelSv = it.label;
        const keel::StringRef catSv = it.category;
        const keel::StringRef descSv = it.description;

        keel::usize labelPos = 0;
        keel::usize labelLen = 0;
        if (TextUtil::FindCaseInsensitive(labelSv, query, labelPos, labelLen)) {
            result.push_back(FilteredCommandEntry{
                .origIndex = keel::as<keel::i32>(i),
                .labelMatchPos = labelPos,
                .labelMatchLen = labelLen
            });
            continue;
        }

        if (TextUtil::FindCaseInsensitive(catSv, query) != keel::kInvalidIndex ||
            TextUtil::FindCaseInsensitive(descSv, query) != keel::kInvalidIndex) {
            result.push_back(FilteredCommandEntry{
                .origIndex = keel::as<keel::i32>(i),
                .labelMatchPos = keel::kInvalidIndex,
                .labelMatchLen = 0
            });
        }
    }
    return result;
}

PaletteLayout ComputePaletteLayout(
    const UiTheme& theme,
    const CommandPaletteOptions& opts,
    Rect backdropRect,
    keel::ArrayRef<CommandPaletteItem> items,
    keel::i32 filteredCount,
    ICanvas* measureCanvas,
    const ScrollbarMetrics& sbm
) {
    PaletteLayout l{};
    l.backdropRect = backdropRect;
    l.showFooter = opts.showFooterHints;

    const UiSize activeSize = opts.size;
    const ComponentMetrics& cm = theme.GetMetrics(activeSize);
    const InputFieldMetrics& im = theme.GetInputField(activeSize);
    const SelectMetrics& sm = theme.GetSelect(activeSize);
    const ChromeMetrics chrome = theme.Painter().GetChromeMetrics(theme);

    l.recessed = chrome.recessedRegions;
    const keel::f32 bevelIn = chrome.frameBevelInset;
    const keel::f32 outerPad = l.recessed ? (bevelIn + 6.0f) : 0.0f;
    const keel::f32 wellIn = l.recessed ? bevelIn : 0.0f;
    const keel::f32 sectionGap = l.recessed ? 6.0f : 1.0f;

    l.searchFont = theme.ResolveFontSize(UiText{}, TextRole::BodyMedium, activeSize);
    l.labelFont  = theme.ResolveFontSize(UiText{}, TextRole::LabelLarge, activeSize);
    l.descFont   = theme.ResolveFontSize(UiText{}, TextRole::BodySmall, activeSize);
    l.badgeFont  = theme.ResolveFontSize(UiText{}, TextRole::LabelSmall, activeSize);

    const keel::f32 searchH = l.recessed
        ? keel::round(keel::max(im.defaultHeight + 2.0f, l.searchFont * 2.1f))
        : keel::round(keel::max(im.defaultHeight + 10.0f, l.searchFont * 2.8f));
    l.rowH     = keel::round(keel::max(sm.itemHeight + 8.0f, l.labelFont * 2.5f));
    l.listPadY = l.recessed
        ? (wellIn + 2.0f)
        : keel::round(keel::max(4.0f, cm.paddingY * 0.75f));
    const keel::f32 footerH = opts.showFooterHints
        ? keel::round(keel::max(cm.defaultHeight * 0.82f, l.badgeFont * 2.5f))
        : 0.0f;

    l.rowInsetX   = l.recessed ? wellIn : 6.0f;
    l.rowPadX     = keel::round(im.paddingX);
    l.rowRightPad = l.recessed ? 10.0f : 16.0f;
    l.rowRadius   = l.recessed ? 0.0f : theme.ResolveRadius(keel::kNullOpt, UiSize::Md);
    l.icSz        = keel::round(cm.iconSize);
    l.catPillH    = keel::round(keel::max(16.0f, l.badgeFont + 7.0f));
    l.keyPillH    = keel::round(keel::max(18.0f, l.badgeFont + 9.0f));

    // Measure uniform tabular category column width across all items (prevents jagged label alignment)
    for (const auto& it : items) {
        if (!it.category.empty()) {
            const keel::f32 cw = TextUtil::MeasureWidth(it.category, l.badgeFont, measureCanvas);
            l.maxCatPillW = keel::max(l.maxCatPillW, keel::ceil(cw + 12.0f));
        }
    }

    const keel::f32 escW = keel::round(keel::max(
        l.recessed ? 40.0f : 32.0f,
        TextUtil::MeasureWidth("ESC", l.badgeFont, measureCanvas) + 14.0f
    ));
    const keel::f32 escH = l.recessed ? searchH : l.keyPillH;
    const keel::f32 clearSz = keel::round(keel::max(20.0f, l.icSz + 6.0f));

    // Measure natural content width so auto-sizing fits icon + category + label + description + shortcut
    keel::f32 maxNaturalW = outerPad * 2.0f + l.rowPadX * 2.0f + l.icSz + 10.0f
        + TextUtil::MeasureWidth(opts.placeholder, l.searchFont, measureCanvas)
        + clearSz + escW + 36.0f;

    for (const auto& it : items) {
        keel::f32 rowW = outerPad * 2.0f + l.rowInsetX * 2.0f + l.rowPadX + 10.0f + l.icSz + 10.0f;
        if (l.maxCatPillW > 0.0f) {
            rowW += l.maxCatPillW + 10.0f;
        }
        rowW += TextUtil::MeasureWidth(it.label, l.labelFont, measureCanvas);
        if (!it.description.empty()) {
            rowW += 12.0f + TextUtil::MeasureWidth(it.description, l.descFont, measureCanvas);
        }
        if (!it.shortcut.empty()) {
            const keel::f32 sw = TextUtil::MeasureWidth(it.shortcut, l.badgeFont, measureCanvas);
            rowW += 12.0f + keel::ceil(sw + 12.0f);
        }
        rowW += 22.0f; // right breathing room + scrollbar gutter reserve
        maxNaturalW = keel::max(maxNaturalW, rowW);
    }

    const keel::f32 minW = keel::max(260.0f, opts.minWidth);
    const keel::f32 maxW = keel::max(minW, opts.maxWidth);
    const keel::f32 targetW = (opts.width.has_value() && *opts.width > 0.0f)
        ? keel::max(260.0f, *opts.width)
        : keel::clamp(keel::ceil(maxNaturalW), minW, maxW);
    const keel::f32 cardW = keel::round(keel::min(keel::max(200.0f, backdropRect.w - 24.0f), targetW));

    l.maxVis = keel::max<keel::u32>(1u, opts.maxVisibleItems);
    const keel::u32 visRows = (filteredCount > 0)
        ? keel::min<keel::u32>(keel::as<keel::u32>(filteredCount), l.maxVis)
        : 1u;
    const keel::f32 listViewportH = (filteredCount > 0)
        ? (keel::as<keel::f32>(visRows) * l.rowH + l.listPadY * 2.0f)
        : keel::round(l.rowH * 1.6f);

    const keel::f32 footerDivH = l.recessed ? 2.0f : 1.0f;
    const keel::f32 cardH = l.recessed
        ? (outerPad + searchH + sectionGap + listViewportH
           + (opts.showFooterHints ? (sectionGap + footerDivH + footerH + bevelIn) : outerPad))
        : (searchH + 1.0f + listViewportH + (opts.showFooterHints ? (1.0f + footerH) : 0.0f));

    const keel::f32 cardX = keel::round(backdropRect.x + (backdropRect.w - cardW) * 0.5f);
    const keel::f32 cardY = keel::round(backdropRect.y + keel::max(20.0f, backdropRect.h * 0.14f));
    l.cardRect = Rect(cardX, cardY, cardW, cardH);

    const keel::f32 contentX = cardX + outerPad;
    const keel::f32 contentW = keel::max(40.0f, cardW - outerPad * 2.0f);

    if (l.recessed) {
        l.searchRect = Rect(contentX, cardY + outerPad, keel::max(40.0f, contentW - escW - 6.0f), searchH);
        l.escRect = Rect(contentX + contentW - escW, l.searchRect.y, escW, escH);
        l.clearRect = Rect(
            l.searchRect.Right() - wellIn - 4.0f - clearSz,
            keel::round(l.searchRect.y + (l.searchRect.h - clearSz) * 0.5f),
            clearSz,
            clearSz
        );
        l.listRect = Rect(contentX, l.searchRect.Bottom() + sectionGap, contentW, listViewportH);
        l.listClipRect = Rect(
            l.listRect.x + wellIn,
            l.listRect.y + wellIn,
            keel::max(0.0f, l.listRect.w - wellIn * 2.0f),
            keel::max(0.0f, l.listRect.h - wellIn * 2.0f)
        );
        l.sbViewport = l.listClipRect;
        l.footerDivY = l.listRect.Bottom() + sectionGap;
        l.footerRect = Rect(cardX + bevelIn, l.footerDivY + footerDivH, keel::max(0.0f, cardW - bevelIn * 2.0f), footerH);
        l.searchIconPad = wellIn + 6.0f;
        l.searchIconGap = 8.0f;
        l.textRightEdgeEmpty = l.searchRect.Right() - wellIn - 6.0f;
        l.divInset = bevelIn;
        l.footerPadX = 6.0f;
    } else {
        l.searchRect = Rect(cardX, cardY, cardW, searchH);
        l.escRect = Rect(
            l.searchRect.Right() - l.rowPadX - escW,
            keel::round(l.searchRect.y + (l.searchRect.h - escH) * 0.5f),
            escW,
            escH
        );
        l.clearRect = Rect(
            l.escRect.x - 6.0f - clearSz,
            keel::round(l.searchRect.y + (l.searchRect.h - clearSz) * 0.5f),
            clearSz,
            clearSz
        );
        l.listRect = Rect(cardX, l.searchRect.Bottom() + sectionGap, cardW, listViewportH);
        l.listClipRect = l.listRect;
        l.sbViewport = Rect(
            l.listRect.x,
            l.listRect.y + l.listPadY - sbm.margin,
            l.listRect.w,
            keel::max(0.0f, l.listRect.h - (l.listPadY - sbm.margin) * 2.0f)
        );
        l.footerDivY = l.listRect.Bottom();
        l.footerRect = Rect(cardX, l.footerDivY + footerDivH, cardW, footerH);
        l.searchIconPad = l.rowPadX + 2.0f;
        l.searchIconGap = 10.0f;
        l.textRightEdgeEmpty = l.escRect.x - 8.0f;
        l.divInset = 1.0f;
        l.footerPadX = l.rowPadX + 2.0f;
    }

    return l;
}

void DrawKeycap(
    ICanvas& c,
    const UiTheme& theme,
    Rect rect,
    keel::StringRef text,
    keel::f32 fontSize,
    Color bg,
    Color preferredFg,
    keel::f32 radius,
    bool hovered = false,
    bool disabled = false,
    bool ensureContrast = true
) {
    const auto& pal = theme.palette;
    theme.Painter().DrawFrame(c, theme, FrameKind::Keycap, FramePaintParams{
        .rect = rect,
        .radius = radius,
        .bg = bg,
        .border = pal.borderSubtle,
        .borderWidth = 1.0f,
        .hovered = hovered,
        .disabled = disabled
    });
    const Color fg = disabled
        ? pal.textDisabled
        : (ensureContrast ? theme.EnsureReadableInk(preferredFg, bg) : preferredFg);
    c.DrawTextBox(text, rect, fontSize, fg, 0.5f, 0.5f);
}

void DrawSearchBar(
    ICanvas& c,
    const UiTheme& theme,
    const PaletteLayout& l,
    keel::StringRef query,
    const UiText& placeholder,
    bool cursorVis,
    bool clearBtnHovered,
    bool escPillHovered
) {
    const auto& pal = theme.palette;
    const Color primaryCol = theme.GetSemanticAccent(UiVariant::Primary);

    if (l.recessed) {
        theme.Painter().DrawFrame(c, theme, FrameKind::Input, FramePaintParams{
            .rect = l.searchRect,
            .bg = pal.inputBackground,
            .border = pal.inputBorder,
            .borderWidth = 1.0f
        });
    }

    const Rect searchIconRect(
        l.searchRect.x + l.searchIconPad,
        keel::round(l.searchRect.y + (l.searchRect.h - l.icSz) * 0.5f),
        l.icSz,
        l.icSz
    );
    Icon::Search().Draw(c, searchIconRect, primaryCol);

    const keel::f32 textStartX = searchIconRect.Right() + l.searchIconGap;
    const keel::f32 textEndX = !query.empty() ? (l.clearRect.x - 6.0f) : l.textRightEdgeEmpty;
    const Rect queryTextRect(textStartX, l.searchRect.y, keel::max(10.0f, textEndX - textStartX), l.searchRect.h);

    c.PushClip(queryTextRect);
    if (query.empty()) {
        const keel::f32 placeholderOffset = 4.0f;
        TextUtil::DrawUiText(
            c,
            placeholder,
            Rect(textStartX + placeholderOffset, l.searchRect.y, keel::max(10.0f, queryTextRect.w - placeholderOffset), l.searchRect.h),
            l.searchFont,
            l.searchRect.h,
            pal.textMuted,
            0.0f,
            0.5f,
            &theme
        );
        if (cursorVis) {
            const keel::f32 caretH = keel::round(l.searchFont * 1.15f);
            const Rect caretRect(textStartX, keel::round(l.searchRect.Center().y - caretH * 0.5f), 1.5f, caretH);
            c.DrawRect(caretRect, primaryCol);
        }
    } else {
        const keel::f32 qw = c.MeasureText(query, l.searchFont).x;
        const keel::f32 drawX = (qw > queryTextRect.w - 4.0f)
            ? (queryTextRect.Right() - qw - 4.0f)
            : textStartX;
        c.DrawTextBox(
            query,
            Rect(drawX, l.searchRect.y, qw + 8.0f, l.searchRect.h),
            l.searchFont,
            pal.textPrimary,
            0.0f,
            0.5f
        );
        if (cursorVis) {
            const keel::f32 caretH = keel::round(l.searchFont * 1.15f);
            const Rect caretRect(
                keel::round(drawX + qw + 1.0f),
                keel::round(l.searchRect.Center().y - caretH * 0.5f),
                1.5f,
                caretH
            );
            c.DrawRect(caretRect, primaryCol);
        }
    }
    c.PopClip();

    // Clear 'x' button when query is non-empty
    if (!query.empty()) {
        if (clearBtnHovered) {
            theme.Painter().DrawFrame(c, theme, FrameKind::ToolbarButton, FramePaintParams{
                .rect = l.clearRect,
                .radius = keel::min(5.0f, theme.GetRadius(UiSize::Sm)),
                .bg = pal.surfaceHover,
                .hovered = true,
                .accent = primaryCol
            });
        }
        const keel::f32 xSz = keel::round(l.icSz * 0.75f);
        const Rect xBox(
            keel::round(l.clearRect.x + (l.clearRect.w - xSz) * 0.5f),
            keel::round(l.clearRect.y + (l.clearRect.h - xSz) * 0.5f),
            xSz,
            xSz
        );
        Icon::Close().Draw(c, xBox, clearBtnHovered ? pal.textPrimary : pal.textMuted);
    }

    // [ESC] button / keycap on the right of search bar
    if (l.recessed) {
        const Color escBg = escPillHovered ? pal.surfaceHover : pal.surface;
        theme.Painter().DrawFrame(c, theme, FrameKind::Button, FramePaintParams{
            .rect = l.escRect,
            .bg = escBg,
            .border = pal.border,
            .borderWidth = 1.0f,
            .hovered = escPillHovered
        });
        c.DrawTextBox("ESC", l.escRect, l.badgeFont, theme.EnsureReadableInk(pal.textPrimary, escBg), 0.5f, 0.5f);
    } else {
        const Color keycapBg = escPillHovered ? pal.surfaceActive : pal.surfaceHover;
        DrawKeycap(c, theme, l.escRect, "ESC", l.badgeFont, keycapBg, pal.textMuted, 4.0f, escPillHovered, false, false);
        theme.Painter().DrawDivider(
            c, theme,
            Vec2(l.cardRect.x + 2.0f, l.searchRect.Bottom()),
            Vec2(l.cardRect.Right() - 2.0f, l.searchRect.Bottom()),
            pal.borderSubtle, 1.0f
        );
    }
}

void DrawCommandList(
    ICanvas& c,
    const UiTheme& theme,
    const PaletteLayout& l,
    keel::ArrayRef<DrawRowItem> drawRows,
    keel::usize totalFiltered,
    const ScrollbarGeometry& sbGeom,
    const ScrollbarState& sbState,
    const ScrollbarMetrics& sbm
) {
    const auto& pal = theme.palette;
    const Color primaryCol = theme.GetSemanticAccent(UiVariant::Primary);

    const SurfacePaintParams listWellParams{
        .rect = l.listRect,
        .role = SurfaceRole::Sunken,
        .bg = pal.inputBackground,
        .border = pal.inputBorder,
        .borderWidth = 1.0f
    };
    if (l.recessed) {
        theme.Painter().DrawSurfaceBg(c, theme, listWellParams);
    }

    c.PushClip(l.listClipRect);

    if (totalFiltered == 0) {
        c.DrawTextBox("No matching commands", l.listClipRect, l.labelFont, pal.textMuted, 0.5f, 0.5f);
    } else {
        for (const auto& row : drawRows) {
            const Rect& rr = row.rowRect;

            const bool solidAccent = theme.Painter().DrawSelectionHighlight(
                c, theme, rr, l.rowRadius,
                /*hovered=*/row.highlighted, /*selected=*/row.highlighted,
                /*subExpanded=*/false, /*isDanger=*/false, row.disabled, Color::Transparent()
            );

            const Color fg = row.disabled ? pal.textDisabled : (solidAccent ? pal.accentText : pal.textPrimary);
            const Color mutedFg = row.disabled ? pal.textDisabled : (solidAccent ? pal.accentText.WithAlphaF(0.85f) : pal.textMuted);

            keel::f32 curX = rr.x + l.rowPadX;
            keel::f32 rightEdge = rr.Right() - l.rowRightPad;

            // Right-aligned [shortcut] keycap badge
            if (!row.shortcut.empty()) {
                const keel::f32 sw = c.MeasureText(row.shortcut, l.badgeFont).x;
                const keel::f32 pillW = keel::round(sw + 12.0f);
                const Rect pillRect(
                    rightEdge - pillW,
                    keel::round(rr.y + (rr.h - l.keyPillH) * 0.5f),
                    pillW,
                    l.keyPillH
                );
                const Color pillBg = l.recessed
                    ? pal.surface
                    : (row.highlighted ? pal.surfaceActive.WithAlphaF(0.85f) : pal.surfaceHover.WithAlphaF(0.75f));
                const Color preferredFg = l.recessed ? pal.textPrimary : pal.textSecondary;
                DrawKeycap(c, theme, pillRect, row.shortcut, l.badgeFont, pillBg, preferredFg, 4.0f, false, row.disabled);
                rightEdge = pillRect.x - 10.0f;
            }

            // Leading icon
            const Rect icRect(curX, keel::round(rr.y + (rr.h - l.icSz) * 0.5f), l.icSz, l.icSz);
            if (row.icon.IsValid()) {
                const Color icCol = row.disabled
                    ? pal.textDisabled
                    : (solidAccent ? fg : (row.highlighted ? primaryCol : mutedFg));
                row.icon.Draw(c, icRect, icCol);
            } else {
                Icon::ChevronRight().Draw(c, icRect, mutedFg.WithAlphaF(0.6f));
            }
            curX += l.icSz + 10.0f;

            // Tabular [category] column prefix (uniform width so all labels align vertically)
            if (l.maxCatPillW > 0.0f) {
                const keel::f32 catColW = keel::min(l.maxCatPillW, keel::max(36.0f, (rightEdge - curX) * 0.30f));
                if (!row.category.empty()) {
                    const Rect catRect(curX, keel::round(rr.y + (rr.h - l.catPillH) * 0.5f), catColW, l.catPillH);
                    const Color catBg = solidAccent
                        ? (l.recessed ? pal.surface : fg.WithAlphaF(0.20f))
                        : primaryCol.WithAlphaF(row.highlighted ? 0.22f : 0.12f);
                    const Color catFg = row.disabled
                        ? pal.textDisabled
                        : (solidAccent
                            ? (l.recessed ? theme.EnsureReadableInk(primaryCol, catBg) : fg)
                            : primaryCol);
                    theme.Painter().DrawBadge(c, theme, catRect, catBg, Color::Transparent(), 0.0f, 4.0f);
                    const auto truncCat = TextUtil::TruncateWithEllipsis(
                        row.category,
                        keel::max(0.0f, catColW - 8.0f),
                        l.badgeFont,
                        &c
                    );
                    c.DrawTextBox(truncCat, catRect, l.badgeFont, catFg, 0.5f, 0.5f);
                }
                curX += catColW + 10.0f;
            }

            // Label with matched substring highlight and graceful ellipsis fallback
            const keel::f32 availTextW = keel::max(0.0f, rightEdge - curX);
            if (availTextW > 4.0f) {
                const keel::f32 fullLabelW = c.MeasureText(row.label, l.labelFont).x;
                const Color matchFill = solidAccent ? pal.accentText.WithAlphaF(0.24f) : primaryCol.WithAlphaF(0.32f);
                const Color matchBorder = solidAccent ? pal.accentText.WithAlphaF(0.55f) : primaryCol.WithAlphaF(0.55f);

                TextUtil::DrawHighlightedTextBox(
                    c,
                    row.label,
                    Rect(curX, rr.y, availTextW, rr.h),
                    l.labelFont,
                    fg,
                    row.matchPos,
                    row.matchLen,
                    matchFill,
                    matchBorder,
                    3.0f,
                    0.5f
                );

                curX += keel::min(fullLabelW, availTextW) + 12.0f;

                if (!row.description.empty() && rightEdge - curX > 24.0f) {
                    const auto truncDesc = TextUtil::TruncateWithEllipsis(
                        row.description,
                        rightEdge - curX,
                        l.descFont,
                        &c
                    );
                    if (!truncDesc.empty()) {
                        c.DrawTextBox(truncDesc, Rect(curX, rr.y, rightEdge - curX, rr.h), l.descFont, mutedFg, 0.0f, 0.5f);
                    }
                }
            }
        }

        Scrollbar::DrawVertical(c, sbGeom, sbState, sbm, theme, primaryCol);
    }

    c.PopClip();

    if (l.recessed) {
        theme.Painter().DrawSurfaceFrame(c, theme, listWellParams);
    }
}

void DrawFooter(
    ICanvas& c,
    const UiTheme& theme,
    const PaletteLayout& l,
    keel::usize totalFiltered
) {
    if (!l.showFooter || l.footerRect.h <= 0.0f) {
        return;
    }

    const auto& pal = theme.palette;
    theme.Painter().DrawDivider(
        c, theme,
        Vec2(l.cardRect.x + l.divInset, l.footerDivY),
        Vec2(l.cardRect.Right() - l.divInset, l.footerDivY),
        pal.borderSubtle, 1.0f
    );

    const Rect footerInner(
        l.footerRect.x + l.footerPadX,
        l.footerRect.y,
        keel::max(0.0f, l.footerRect.w - l.footerPadX * 2.0f),
        l.footerRect.h
    );

    char countBuf[48]{};
    const keel::StringRef countLabel = (totalFiltered == 1)
        ? keel::StringRef("1 command")
        : keel::format_buf(countBuf, "%zu commands", totalFiltered);
    const keel::f32 countW = c.MeasureText(countLabel, l.badgeFont).x;
    c.DrawTextBox(countLabel, footerInner, l.badgeFont, pal.textMuted, 1.0f, 0.5f);

    struct FooterHint {
        keel::StringRef key;
        keel::StringRef action;
    };
    static constexpr keel::StaticArray<FooterHint, 3> kHints = {{
        {"↑↓",    "Navigate"},
        {"Enter", "Execute"},
        {"Esc",   "Close"}
    }};

    keel::f32 hintX = footerInner.x;
    const keel::f32 maxHintRight = footerInner.Right() - countW - 12.0f;
    const keel::f32 kcapH = keel::round(keel::max(16.0f, l.badgeFont + 5.0f));
    const keel::f32 kcapY = keel::round(l.footerRect.y + (l.footerRect.h - kcapH) * 0.5f);
    const Color kcapBg = l.recessed ? pal.surface : pal.surfaceHover;
    const Color kcapFg = l.recessed ? pal.textPrimary : pal.textSecondary;

    for (const auto& h : kHints) {
        const keel::f32 kw = c.MeasureText(h.key, l.badgeFont).x;
        const keel::f32 kcapW = keel::round(kw + 10.0f);
        const keel::f32 aw = c.MeasureText(h.action, l.badgeFont).x;
        const keel::f32 groupW = kcapW + 5.0f + aw;
        if (hintX + groupW > maxHintRight) {
            break;
        }
        DrawKeycap(c, theme, Rect(hintX, kcapY, kcapW, kcapH), h.key, l.badgeFont, kcapBg, kcapFg, 3.5f, false, false, false);
        c.DrawTextBox(h.action, Rect(hintX + kcapW + 5.0f, l.footerRect.y, aw + 4.0f, l.footerRect.h), l.badgeFont, pal.textMuted, 0.0f, 0.5f);
        hintX += groupW + 14.0f;
    }
}

} // namespace

CommandPaletteResponse detail::CommandPalette::Run(
    Context& ctx,
    bool& isOpen,
    keel::ArrayRef<CommandPaletteItem> items,
    const CommandPaletteOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    auto& st = ctx.GetOrCreatePersistentState<CommandPalettePersistState>(id);

    CommandPaletteResponse res{};
    res.id = id;
    res.ctx = &ctx;
    res.selectedIndex = -1;

    if (!isOpen) {
        if (st.wasOpen) {
            st.wasOpen = false;
            st.scrollbar = {};
            ctx.InvalidateAll();
        }
        return res;
    }

    const UiSize activeSize = opts.size;
    const UiTheme theme = ctx.GetTheme();

    const bool justOpened = !st.wasOpen;
    if (justOpened) {
        ctx.ClosePopup();
        st.query.clear();
        st.highlightedPos = 0;
        st.scrollOffset = 0.0f;
        st.scrollbar = {};
        st.lastMousePos = ctx.GetMousePos();
        st.wasOpen = true;
        ctx.InvalidateAll();
    }

    const bool deferKeysToPopup = ctx.GetOpenPopupId().IsValid() || (!justOpened && ctx.WasPopupDismissedThisFrame());

    // Ensure no underlying InputField captures text input while the CommandPalette owns focus
    if (!deferKeysToPopup) {
        ctx.ClearFocus();
    }

    // 1. Process text input & editing keys
    bool queryModified = false;

    if (!deferKeysToPopup) {
        KeyModifiers bsMods{};
        while (ctx.ConsumeKey(Key::Backspace, bsMods)) {
            if (!st.query.empty()) {
                if (bsMods.ctrl) {
                    st.query.clear();
                } else {
                    const keel::usize prev = TextUtil::PrevCharBoundary(st.query.as_ref(), st.query.size());
                    st.query.resize(prev);
                }
                queryModified = true;
            }
        }

        const keel::Array<keel::String> pendingText = ctx.ConsumePendingTextInput();
        for (const keel::String& chunk : pendingText) {
            for (char c : chunk) {
                const keel::u8 uc = keel::as<keel::u8>(c);
                if (uc >= 32 && uc != 127) {
                    st.query.push_back(c);
                    queryModified = true;
                }
            }
        }
    }

    if (queryModified) {
        st.highlightedPos = 0;
        st.scrollOffset = 0.0f;
        ctx.InvalidateAll();
    }

    // 2. Filter items against current query
    keel::SmallArray<FilteredCommandEntry, 64> filtered = FilterItems(items, st.query.as_ref());
    const keel::i32 filteredCount = keel::as<keel::i32>(filtered.size());

    if (filteredCount == 0) {
        st.highlightedPos = -1;
        st.scrollOffset = 0.0f;
    } else {
        st.highlightedPos = keel::clamp(st.highlightedPos, 0, filteredCount - 1);
    }

    // 3. Compute unified layout from UiSize + ChromeMetrics + content-aware natural row widths
    Rect backdropRect = ctx.GetPopupViewportRect();
    if (opts.confineToParent) {
        const Rect parentRect = ctx.GetParentPrevFrameBounds();
        if (parentRect.w > 10.0f && parentRect.h > 10.0f) {
            backdropRect = parentRect;
        }
    }

    ScrollbarMetrics sbm = theme.GetScrollbar(activeSize);
    const PaletteLayout layout = ComputePaletteLayout(
        theme, opts, backdropRect, items, filteredCount, ctx.GetCanvas(), sbm
    );
    sbm.scrollStep = keel::max(sbm.scrollStep, layout.rowH * 1.25f);

    const keel::f32 totalListContentH = (filteredCount > 0) ? (keel::as<keel::f32>(filteredCount) * layout.rowH) : 0.0f;
    const keel::f32 innerListViewportH = keel::max(0.0f, layout.listRect.h - layout.listPadY * 2.0f);
    const keel::f32 maxScroll = keel::max(0.0f, totalListContentH - innerListViewportH);

    auto scrollHighlightedIntoView = [&]() {
        if (st.highlightedPos < 0 || filteredCount <= 0) return;
        const keel::f32 itemTop = keel::as<keel::f32>(st.highlightedPos) * layout.rowH;
        const keel::f32 itemBottom = itemTop + layout.rowH;
        if (itemTop < st.scrollOffset) {
            st.scrollOffset = itemTop;
        } else if (itemBottom > st.scrollOffset + innerListViewportH) {
            st.scrollOffset = itemBottom - innerListViewportH;
        }
        st.scrollOffset = keel::clamp(st.scrollOffset, 0.0f, maxScroll);
    };

    // 4. Keyboard navigation & execution
    auto moveHighlight = [&](keel::i32 delta) {
        if (filteredCount <= 0) return;
        st.highlightedPos = keel::clamp(st.highlightedPos + delta, 0, filteredCount - 1);
        scrollHighlightedIntoView();
        ctx.InvalidateAll();
    };

    if (!deferKeysToPopup) {
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
            moveHighlight(-keel::as<keel::i32>(layout.maxVis));
        } else if (ctx.ConsumeKey(Key::PageDown)) {
            moveHighlight(keel::as<keel::i32>(layout.maxVis));
        }

        if (opts.closeOnEscape && ctx.ConsumeKey(Key::Escape)) {
            isOpen = false;
            st.wasOpen = false;
            ctx.InvalidateAll();
            return res;
        }

        if (ctx.ConsumeKey(Key::Enter)) {
            if (st.highlightedPos >= 0 && st.highlightedPos < filteredCount) {
                const keel::i32 origIdx = filtered[st.highlightedPos].origIndex;
                if (origIdx >= 0 && keel::as<keel::usize>(origIdx) < items.size() && !items[keel::as<keel::usize>(origIdx)].disabled) {
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
    }

    // 5. Mouse wheel, interactive scrollbar & hit testing
    const Vec2 mouse = ctx.GetMousePos();
    const bool mouseMoved = (keel::abs(mouse.x - st.lastMousePos.x) > 0.5f || keel::abs(mouse.y - st.lastMousePos.y) > 0.5f);
    st.lastMousePos = mouse;

    ScrollbarGeometry sbGeom{};
    bool clearBtnHovered = false;
    bool escPillHovered = false;
    bool mouseOccluded = false;

    {
        auto hitScope = ctx.ScopedOverlayHitCheck(id, OverlayLayer::Portal);
        mouseOccluded = ctx.IsOverlayOccluded(id, mouse, OverlayLayer::Portal);

        if (mouseOccluded) {
            sbGeom = Scrollbar::ComputeVertical(
                layout.sbViewport,
                totalListContentH,
                st.scrollOffset,
                sbm,
                theme.scrollbarStyle
            );
        } else {
            sbGeom = Scrollbar::UpdateVertical(
                ctx,
                id,
                layout.sbViewport,
                totalListContentH,
                st.scrollOffset,
                st.scrollbar,
                sbm,
                backdropRect
            );

            clearBtnHovered = !st.query.empty() && layout.clearRect.Contains(mouse);
            escPillHovered = opts.closeOnEscape && layout.escRect.Contains(mouse);
        }
        if (clearBtnHovered != st.prevClearHovered || escPillHovered != st.prevEscHovered) {
            st.prevClearHovered = clearBtnHovered;
            st.prevEscHovered = escPillHovered;
            ctx.InvalidateAll();
        }

        keel::i32 hoveredFilteredPos = -1;
        if (!mouseOccluded && filteredCount > 0 && layout.listClipRect.Contains(mouse) && !st.scrollbar.hovered && !st.scrollbar.dragging) {
            const keel::f32 relY = (mouse.y - (layout.listRect.y + layout.listPadY)) + st.scrollOffset;
            if (relY >= 0.0f) {
                const keel::i32 idx = keel::as<keel::i32>(relY / layout.rowH);
                if (idx >= 0 && idx < filteredCount) {
                    hoveredFilteredPos = idx;
                    if (mouseMoved && st.highlightedPos != idx) {
                        st.highlightedPos = idx;
                        ctx.InvalidateAll();
                    }
                }
            }
        }

        if (!mouseOccluded && ctx.IsMouseClicked()) {
            if (layout.cardRect.Contains(mouse)) {
                if (clearBtnHovered) {
                    st.query.clear();
                    st.highlightedPos = 0;
                    st.scrollOffset = 0.0f;
                    ctx.ConsumeMouseClick();
                    ctx.InvalidateAll();
                    filtered = FilterItems(items, st.query.as_ref());
                    sbGeom = Scrollbar::ComputeVertical(
                        layout.sbViewport,
                        keel::as<keel::f32>(filtered.size()) * layout.rowH,
                        st.scrollOffset,
                        sbm,
                        theme.scrollbarStyle
                    );
                } else if (escPillHovered) {
                    isOpen = false;
                    st.wasOpen = false;
                    st.scrollbar = {};
                    ctx.ConsumeMouseClick();
                    ctx.InvalidateAll();
                    return res;
                } else if (hoveredFilteredPos >= 0 && hoveredFilteredPos < keel::as<keel::i32>(filtered.size())) {
                    const keel::i32 origIdx = filtered[hoveredFilteredPos].origIndex;
                    if (origIdx >= 0 && keel::as<keel::usize>(origIdx) < items.size() && !items[keel::as<keel::usize>(origIdx)].disabled) {
                        res.selectedIndex = origIdx;
                        res.changed = true;
                        res.clicked = true;
                        isOpen = false;
                        st.wasOpen = false;
                        st.scrollbar = {};
                        ctx.ConsumeMouseClick();
                        ctx.InvalidateAll();
                        return res;
                    }
                    ctx.ConsumeMouseClick();
                } else {
                    ctx.ConsumeMouseClick();
                }
            } else if (opts.closeOnBackdropClick && backdropRect.Contains(mouse) && !st.scrollbar.dragging) {
                isOpen = false;
                st.wasOpen = false;
                st.scrollbar = {};
                ctx.ConsumeMouseClick();
                ctx.InvalidateAll();
                return res;
            }
        }
    }

    // Block underlying widgets from receiving hover/clicks inside backdropRect (below Popup layer)
    ctx.RegisterOverlayHitRegion(id, backdropRect, OverlayLayer::Portal);
    ctx.RegisterOverlayHitBuilder([&ctx, id, backdropRect]() {
        ctx.RegisterOverlayHitRegion(id, backdropRect, OverlayLayer::Portal);
    });

    res.rect = layout.cardRect;
    res.hovered = !mouseOccluded && layout.cardRect.Contains(mouse);

    // 6. Snapshot visible rows into frame arena for deferred overlay rendering
    const keel::f32 sbRightReserve = sbGeom.visible ? (sbGeom.trackRect.w + sbm.margin) : 0.0f;
    const ScrollbarState sbStateCopy = st.scrollbar;

    DrawRowItem* arenaRows = ctx.GetArena().create_array<DrawRowItem>(filtered.size());
    keel::usize drawRowCount = 0;
    for (keel::usize i = 0; i < filtered.size(); ++i) {
        const auto& fe = filtered[i];
        const auto& src = items[keel::as<keel::usize>(fe.origIndex)];
        const keel::f32 rowY = layout.listRect.y + layout.listPadY + keel::as<keel::f32>(i) * layout.rowH - st.scrollOffset;
        if (rowY + layout.rowH < layout.listRect.y || rowY > layout.listRect.Bottom()) {
            continue;
        }
        arenaRows[drawRowCount++] = DrawRowItem{
            .label = ctx.AllocString(src.label),
            .category = ctx.AllocString(src.category),
            .description = ctx.AllocString(src.description),
            .shortcut = ctx.AllocString(src.shortcut),
            .icon = ctx.AllocIcon(src.icon),
            .disabled = src.disabled,
            .highlighted = (keel::as<keel::i32>(i) == st.highlightedPos),
            .matchPos = fe.labelMatchPos,
            .matchLen = fe.labelMatchLen,
            .rowRect = Rect(
                layout.listRect.x + layout.rowInsetX,
                keel::round(rowY),
                layout.listRect.w - layout.rowInsetX * 2.0f - sbRightReserve,
                layout.rowH
            )
        };
    }
    const keel::ArrayRef<DrawRowItem> drawRows(arenaRows, drawRowCount);

    const keel::StringRef queryCopy = ctx.AllocString(st.query.as_ref());
    const UiText placeholderCopy = ctx.AllocText(opts.placeholder);
    const keel::f32 backdropOpacity = opts.backdropOpacity;
    const keel::Optional<Color> backdropColor = opts.backdropColor;
    const StateColor bgOpt = opts.background;
    const Border borderOpt = opts.border;
    const bool cursorVis = ctx.IsCursorVisible();
    if (cursorVis != st.prevCursorVis) {
        st.prevCursorVis = cursorVis;
        ctx.InvalidateAll();
    }
    const keel::usize totalFiltered = filtered.size();

    ctx.MarkModalOverlayActive();
    ctx.RegisterOverlay([
        layout,
        queryCopy,
        placeholderCopy,
        drawRows,
        totalFiltered,
        sbGeom,
        sbm,
        sbStateCopy,
        clearBtnHovered,
        escPillHovered,
        backdropOpacity,
        backdropColor,
        bgOpt,
        borderOpt,
        cursorVis,
        theme
    ](ICanvas& c) {
        const auto& pal = theme.palette;

        // 1. Dimmed backdrop
        if (backdropColor.has_value()) {
            if (backdropColor->a > 0 && layout.backdropRect.w > 0.0f && layout.backdropRect.h > 0.0f) {
                c.DrawRect(layout.backdropRect, *backdropColor);
            }
        } else {
            const keel::f32 alpha = keel::clamp(backdropOpacity, 0.0f, 0.95f);
            if (alpha > 0.001f && layout.backdropRect.w > 0.0f && layout.backdropRect.h > 0.0f) {
                c.DrawRect(
                    layout.backdropRect,
                    Color(0, 0, 0, keel::as<keel::u8>(keel::clamp(keel::round_i32(alpha * 255.0f), 0, 255)))
                );
            }
        }

        // 2. Elevated popup surface via active IStylePainter
        const keel::f32 radius = theme.ResolveRadius(borderOpt.radius, UiSize::Lg);
        const keel::f32 borderW = (borderOpt.width > 0.0f) ? borderOpt.width : 1.0f;
        const Color defaultSurface = layout.recessed ? pal.surface : pal.surfaceElevated.WithAlpha(252u);
        const Color surfaceBg = bgOpt.HasAny()
            ? theme.ResolveColor(bgOpt, false, false, false, defaultSurface)
            : defaultSurface;
        const Color borderCol = borderOpt.color.HasAny()
            ? theme.ResolveColor(borderOpt.color, false, false, false, pal.border)
            : pal.border;

        theme.Painter().DrawSurface(c, theme, SurfacePaintParams{
            .rect = layout.cardRect,
            .role = SurfaceRole::Popup,
            .radius = radius,
            .explicitRadius = borderOpt.radius.has_value(),
            .bg = surfaceBg,
            .border = borderCol,
            .borderWidth = borderW,
            .explicitBorder = borderOpt.HasExplicitStroke(),
            .elevation = Elevation::Lg,
            .shadowScale = 1.25f
        });

        // 3. Search Bar, Command List, and Footer Hints
        DrawSearchBar(c, theme, layout, queryCopy, placeholderCopy, cursorVis, clearBtnHovered, escPillHovered);
        DrawCommandList(c, theme, layout, drawRows, totalFiltered, sbGeom, sbStateCopy, sbm);
        DrawFooter(c, theme, layout, totalFiltered);
    }, OverlayLayer::Portal);

    return res;
}

} // namespace guikit
