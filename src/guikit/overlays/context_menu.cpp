#include "guikit/overlays/context_menu.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/style/style_painter.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/array.h"
#include "keel/math.h"
#include "keel/small_array.h"

namespace guikit {

namespace {

constexpr keel::usize kMaxMenuDepth = 8;

struct PrevMenuItemHit {
    Id id{Id::Null()};
    Rect rect{};
    keel::String labelLowerPrefix{}; // First lowercase ASCII/UTF-8 byte for type-ahead
    char firstAsciiLower{0};
    bool isSubMenu{false};
    bool disabled{false};
    bool nonSelectable{false}; // Separator or section header
    bool keepOpen{false};
    keel::i32 flatIndex{-1};
};

struct PrevMenuPanel {
    Id panelId{Id::Null()};
    Rect popupRect{};
    Rect contentRect{};
    keel::f32 scrollY{0.0f};
    keel::f32 maxScrollY{0.0f};
    keel::Array<PrevMenuItemHit> items{};
};

struct MenuTreePersistState {
    Vec2 rootAnchorPos{0.0f, 0.0f};
    keel::Optional<Vec2> lastCallerAnchorPos{keel::kNullOpt};
    Rect anchorRect{};
    bool wasOpen{false};

    // Open submenu chain (index 0 = rootId, index 1 = depth-1 submenu Id, etc.)
    keel::SmallArray<Id, kMaxMenuDepth> openPath{};
    // Highlighted item per depth level (tracks keyboard focus & active parent submenu row)
    keel::SmallArray<Id, kMaxMenuDepth> highlightPath{};
    // Scroll offsets preserved per panel depth
    keel::SmallArray<keel::f32, kMaxMenuDepth> scrollYByDepth{};
    // Layout snapshot from the previous frame for zero-lag hit testing & keyboard nav
    keel::SmallArray<PrevMenuPanel, kMaxMenuDepth> prevPanels{};

    // Hover-intent safe-triangle state
    Vec2 lastMousePos{-1e6f, -1e6f};
    Vec2 submenuSafeOrigin{-1e6f, -1e6f};
    keel::f64 submenuGraceUntilSec{0.0};
    keel::u32 submenuGraceDepth{0};
    bool pendingFocusDeepest{false};
};

char ToLowerAscii(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? keel::as<char>(c - 'A' + 'a') : c;
}

bool PointInTriangle(Vec2 p, Vec2 a, Vec2 b, Vec2 c) noexcept {
    auto sign = [](Vec2 p1, Vec2 p2, Vec2 p3) noexcept -> keel::f32 {
        return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
    };
    const keel::f32 d1 = sign(p, a, b);
    const keel::f32 d2 = sign(p, b, c);
    const keel::f32 d3 = sign(p, c, a);
    const bool hasNeg = (d1 < 0.0f) || (d2 < 0.0f) || (d3 < 0.0f);
    const bool hasPos = (d1 > 0.0f) || (d2 > 0.0f) || (d3 > 0.0f);
    return !(hasNeg && hasPos);
}

bool HalfOpenContains(const Rect& r, Vec2 pt) noexcept {
    return r.w > 0.0f && r.h > 0.0f &&
           pt.x >= r.x && pt.x < r.Right() &&
           pt.y >= r.y && pt.y < r.Bottom();
}

Id MakeMenuItemId(Id panelId, keel::u32 ordinal, keel::StringRef label) noexcept {
    return Id(detail::HashArgs(panelId.value, ordinal, label));
}

struct RecordedMenuItem {
    Id id{Id::Null()};
    keel::StringRef label{};
    keel::StringRef shortcut{};
    Icon icon{};
    UiVariant variant{UiVariant::Default};
    bool checked{false};
    bool hasCheck{false};
    bool isRadio{false};
    bool disabled{false};
    bool isSeparator{false};
    bool isSection{false};
    bool isSubMenu{false};
    bool keepOpen{false};
    keel::u32 childPanelIdx{0}; // Non-zero if this submenu is expanded this frame
};

struct RecordedMenuPanel {
    Id panelId{Id::Null()};
    keel::u32 depth{0};
    keel::u32 parentPanelIdx{0};
    keel::u32 parentItemIdx{0};
    keel::StringRef title{};
    keel::SmallArray<RecordedMenuItem, 16> items{};
};

} // namespace

namespace detail {

struct MenuTreeFrameSession {
    Context* ctx{nullptr};
    Id rootId{Id::Null()};
    bool* openPtr{nullptr};
    MenuTreePersistState* persist{nullptr};
    ContextMenuOptions opts{};
    keel::i32* outHorizontalNav{nullptr};

    Id activatedItemId{Id::Null()};
    bool activatedKeepOpen{false};
    bool closeRequested{false};
    bool anyActivated{false};
    bool callerMovedAnchor{false};
    keel::i32 horizontalNavRequest{0};

    keel::SmallArray<RecordedMenuPanel, kMaxMenuDepth> panels{};
    keel::SmallArray<keel::u32, kMaxMenuDepth> itemOrdinals{};
};

} // namespace detail

namespace {

void ResetPersistState(MenuTreePersistState& st) noexcept {
    st.wasOpen = false;
    st.lastCallerAnchorPos.reset();
    st.openPath.clear();
    st.highlightPath.clear();
    st.scrollYByDepth.clear();
    st.prevPanels.clear();
    st.submenuGraceUntilSec = 0.0;
    st.pendingFocusDeepest = false;
}

keel::i32 FindPrevItemIndex(const PrevMenuPanel& p, Id itemId) noexcept {
    for (keel::usize i = 0; i < p.items.size(); ++i) {
        if (p.items[i].id == itemId) {
            return keel::as<keel::i32>(i);
        }
    }
    return -1;
}

keel::i32 FindFirstSelectable(const PrevMenuPanel& p) noexcept {
    for (keel::usize i = 0; i < p.items.size(); ++i) {
        if (!p.items[i].nonSelectable && !p.items[i].disabled) {
            return keel::as<keel::i32>(i);
        }
    }
    return -1;
}

keel::i32 FindLastSelectable(const PrevMenuPanel& p) noexcept {
    for (keel::usize i = p.items.size(); i > 0; --i) {
        if (!p.items[i - 1].nonSelectable && !p.items[i - 1].disabled) {
            return keel::as<keel::i32>(i - 1);
        }
    }
    return -1;
}

keel::i32 StepSelectable(const PrevMenuPanel& p, Id currentId, keel::i32 dir) noexcept {
    const keel::i32 n = keel::as<keel::i32>(p.items.size());
    if (n <= 0) return -1;
    keel::i32 cur = FindPrevItemIndex(p, currentId);
    for (keel::i32 step = 0; step < n; ++step) {
        cur = (cur < 0)
            ? (dir > 0 ? 0 : n - 1)
            : ((cur + dir + n) % n);
        const auto& it = p.items[keel::as<keel::usize>(cur)];
        if (!it.nonSelectable && !it.disabled) {
            return cur;
        }
    }
    return -1;
}

void EnsureHighlightVisible(MenuTreePersistState& persist, keel::usize depth, const PrevMenuPanel& panel, Id targetId) noexcept {
    if (panel.maxScrollY <= 0.5f || depth >= persist.scrollYByDepth.size()) {
        return;
    }
    const keel::i32 idx = FindPrevItemIndex(panel, targetId);
    if (idx < 0) return;
    const Rect& ir = panel.items[keel::as<keel::usize>(idx)].rect;
    keel::f32& scrollY = persist.scrollYByDepth[depth];
    if (ir.y < panel.contentRect.y) {
        scrollY = keel::max(0.0f, scrollY - (panel.contentRect.y - ir.y));
    } else if (ir.Bottom() > panel.contentRect.Bottom()) {
        scrollY = keel::min(panel.maxScrollY, scrollY + (ir.Bottom() - panel.contentRect.Bottom()));
    }
}

void TruncateOpenPathToDepth(MenuTreePersistState& persist, keel::usize keepLen) noexcept {
    while (persist.openPath.size() > keepLen) {
        persist.openPath.pop_back();
    }
    while (persist.highlightPath.size() > keepLen) {
        persist.highlightPath.pop_back();
    }
    while (persist.scrollYByDepth.size() > keepLen) {
        persist.scrollYByDepth.pop_back();
    }
}

void FinalizeMenuSession(detail::MenuTreeFrameSession& sess) {
    Context& ctx = *sess.ctx;
    MenuTreePersistState& persist = *sess.persist;
    const ContextMenuOptions& opts = sess.opts;

    if (sess.outHorizontalNav != nullptr) {
        *sess.outHorizontalNav = sess.horizontalNavRequest;
    }

    if (sess.closeRequested || sess.horizontalNavRequest != 0) {
        if (ctx.IsPopupOpen(sess.rootId)) {
            ctx.ClosePopup();
        }
        if (sess.openPtr != nullptr) {
            *sess.openPtr = false;
        }
        ResetPersistState(persist);
        ctx.InvalidateAll();
        return;
    }

    if (sess.panels.empty()) {
        return;
    }

    const UiTheme theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();
    const UiSize activeSize = opts.size;
    constexpr keel::f32 s = 1.0f;
    const ComponentMetrics& cm = theme.GetMetrics(activeSize);
    const keel::f32 rowH = keel::round(keel::max(30.0f * s, cm.defaultHeight * 0.88f));
    const keel::f32 sectionH = keel::round(24.0f * s);
    const keel::f32 sepH = keel::round(9.0f * s);
    const keel::f32 padX = keel::round(12.0f * s);
    const keel::f32 padY = keel::round(6.0f * s);
    const keel::f32 icSz = cm.iconSize;
    const keel::f32 iconSlotW = keel::round(icSz + 8.0f * s);
    const keel::f32 submenuSlotW = keel::round(icSz + 6.0f * s);
    const keel::f32 shortcutGap = keel::round(24.0f * s);
    const keel::f32 scrollbarW = 6.0f * s;
    const keel::f32 labelFont = theme.ResolveFontSize(UiText{}, TextRole::LabelLarge, activeSize);
    const keel::f32 shortcutFont = theme.ResolveFontSize(UiText{}, TextRole::LabelMedium, activeSize);
    const keel::f32 sectionFont = theme.ResolveFontSize(UiText{}, TextRole::LabelSmall, activeSize);
    const keel::f32 titleFont = theme.ResolveFontSize(opts.title, TextRole::LabelMedium, activeSize);

    const Rect vp = ctx.GetPopupViewportRect();
    const keel::f32 maxAllowedH = opts.maxHeight.has_value()
        ? keel::max(80.0f, *opts.maxHeight)
        : (vp.h > 60.0f ? keel::max(120.0f, vp.h - 16.0f) : 600.0f);

    struct DrawRow {
        Id id{Id::Null()};
        keel::StringRef label{};
        keel::StringRef shortcut{};
        Icon icon{};
        UiVariant variant{UiVariant::Default};
        bool checked{false};
        bool hasCheck{false};
        bool isRadio{false};
        bool disabled{false};
        bool isSeparator{false};
        bool isSection{false};
        bool isSubMenu{false};
        bool hovered{false};
        bool subExpanded{false};
        Rect rect{};
    };

    struct DrawPanel {
        Rect popupRect{};
        Rect contentRect{};
        keel::StringRef title{};
        keel::f32 titleH{0.0f};
        keel::f32 titleSepY{0.0f};
        keel::f32 scrollY{0.0f};
        keel::f32 maxScrollY{0.0f};
        keel::f32 contentTotalH{0.0f};
        bool anyLeadingSlot{false};
        bool anyTrailingCheck{false};
        bool anySubMenu{false};
        keel::ArrayRef<DrawRow> rows{};
    };

    // Collapse duplicate / leading / trailing separators in each panel cleanly
    for (auto& panel : sess.panels) {
        keel::SmallArray<RecordedMenuItem, 16> cleaned;
        cleaned.reserve(panel.items.size());
        for (const auto& it : panel.items) {
            if (it.isSeparator) {
                if (cleaned.empty() || cleaned.back().isSeparator || cleaned.back().isSection) {
                    continue;
                }
            }
            cleaned.push_back(it);
        }
        while (!cleaned.empty() && cleaned.back().isSeparator) {
            cleaned.pop_back();
        }
        panel.items = keel::move(cleaned);
    }

    // Ensure scrollYByDepth matches panel count
    while (persist.scrollYByDepth.size() < sess.panels.size()) {
        persist.scrollYByDepth.push_back(0.0f);
    }

    // First compute layout of every open panel (root -> child submenus in depth order)
    keel::SmallArray<DrawPanel, kMaxMenuDepth> computedPanels;
    computedPanels.reserve(sess.panels.size());

    // Keep item rects per panel to anchor child submenus precisely
    keel::SmallArray<keel::SmallArray<Rect, 16>, kMaxMenuDepth> panelItemRects;
    panelItemRects.reserve(sess.panels.size());

    persist.prevPanels.clear();

    const bool fromAnchorRect = !opts.anchorPos.has_value() && persist.anchorRect.h > 0.0f;

    for (keel::usize pIdx = 0; pIdx < sess.panels.size(); ++pIdx) {
        const RecordedMenuPanel& rec = sess.panels[pIdx];
        const bool hasTitle = !rec.title.empty();
        const keel::f32 titleH = hasTitle ? keel::round(keel::max(26.0f * s, titleFont + 14.0f * s)) : 0.0f;
        const keel::f32 titleSepH = hasTitle ? keel::round(7.0f * s) : 0.0f;

        bool anyLeadingSlot = false;
        bool anyTrailingCheck = false;
        bool anySubMenu = false;
        for (const auto& it : rec.items) {
            if (it.isSeparator || it.isSection) continue;
            if (it.icon.IsValid() || it.hasCheck || it.checked) {
                anyLeadingSlot = true;
                if (it.icon.IsValid() && (it.hasCheck || it.checked)) {
                    anyTrailingCheck = true;
                }
            }
            if (it.isSubMenu) {
                anySubMenu = true;
            }
        }

        keel::f32 maxContentW = hasTitle ? TextUtil::MeasureUiTextWidth(UiText(rec.title), titleFont, canvas) : 0.0f;
        keel::f32 itemsTotalH = 0.0f;
        for (const auto& it : rec.items) {
            if (it.isSeparator) {
                itemsTotalH += sepH;
                continue;
            }
            if (it.isSection) {
                itemsTotalH += sectionH;
                const keel::f32 sw = TextUtil::MeasureUiTextWidth(UiText(it.label), sectionFont, canvas);
                maxContentW = keel::max(maxContentW, sw);
                continue;
            }
            itemsTotalH += rowH;
            const keel::f32 lw = TextUtil::MeasureUiTextWidth(UiText(it.label), labelFont, canvas);
            const keel::f32 sw = it.shortcut.empty()
                ? 0.0f
                : (shortcutGap + TextUtil::MeasureUiTextWidth(UiText(it.shortcut), shortcutFont, canvas));
            const keel::f32 leadW = anyLeadingSlot ? iconSlotW : 0.0f;
            const keel::f32 trailW = (anyTrailingCheck ? iconSlotW : 0.0f) + (anySubMenu ? submenuSlotW : 0.0f);
            maxContentW = keel::max(maxContentW, leadW + lw + sw + trailW);
        }

        const keel::f32 headerBlockH = titleH + titleSepH;
        const keel::f32 naturalH = padY * 2.0f + headerBlockH + keel::max(rowH, itemsTotalH);
        const bool needsScroll = (naturalH > maxAllowedH + 0.5f);
        const keel::f32 menuH = needsScroll ? keel::round(maxAllowedH) : keel::round(naturalH);

        keel::f32 menuW = maxContentW + padX * 2.0f + 8.0f * s + (needsScroll ? scrollbarW : 0.0f);
        if (pIdx == 0) {
            if (opts.minWidth.has_value()) {
                menuW = keel::max(menuW, *opts.minWidth);
            }
            if (opts.width.has_value() && *opts.width > 0.0f) {
                menuW = *opts.width;
            }
        } else {
            menuW = keel::max(menuW, 168.0f * s);
        }
        menuW = keel::round(menuW);

        // Compute anchored position with auto-flip
        keel::f32 px = 0.0f;
        keel::f32 py = 0.0f;
        if (pIdx == 0) {
            px = persist.rootAnchorPos.x;
            py = persist.rootAnchorPos.y;
            if (vp.w > 0.0f && px + menuW > vp.Right() - 6.0f) {
                if (fromAnchorRect && persist.anchorRect.w > 0.0f) {
                    px = persist.anchorRect.Right() - menuW;
                } else {
                    px = persist.rootAnchorPos.x - menuW;
                }
            }
            if (vp.h > 0.0f && py + menuH > vp.Bottom() - 6.0f) {
                if (fromAnchorRect && persist.anchorRect.h > 0.0f && persist.anchorRect.y - menuH - 4.0f >= vp.y + 6.0f) {
                    py = persist.anchorRect.y - menuH - 4.0f;
                } else {
                    py = vp.Bottom() - menuH - 6.0f;
                }
            }
        } else {
            const Rect& parentPopup = computedPanels[rec.parentPanelIdx].popupRect;
            const Rect& parentRow = (rec.parentItemIdx < panelItemRects[rec.parentPanelIdx].size())
                ? panelItemRects[rec.parentPanelIdx][rec.parentItemIdx]
                : parentPopup;
            // Open to the right of parent menu with a clean 4dp breathing gap; flip to left if clipped
            constexpr keel::f32 kSubmenuGap = 4.0f;
            px = parentPopup.Right() + kSubmenuGap * s;
            if (vp.w > 0.0f && px + menuW > vp.Right() - 6.0f) {
                const keel::f32 flippedX = parentPopup.x - menuW - kSubmenuGap * s;
                if (flippedX >= vp.x + 6.0f || (parentPopup.x - vp.x) > (vp.Right() - parentPopup.Right())) {
                    px = flippedX;
                }
            }
            py = parentRow.y - padY;
            if (vp.h > 0.0f && py + menuH > vp.Bottom() - 6.0f) {
                py = vp.Bottom() - menuH - 6.0f;
            }
        }

        if (vp.w > 0.0f) {
            px = keel::clamp(px, vp.x + 6.0f, keel::max(vp.x + 6.0f, vp.Right() - menuW - 6.0f));
        }
        if (vp.h > 0.0f) {
            py = keel::clamp(py, vp.y + 6.0f, keel::max(vp.y + 6.0f, vp.Bottom() - menuH - 6.0f));
        }

        const Rect popupRect(keel::round(px), keel::round(py), menuW, menuH);
        const Rect contentRect(
            popupRect.x,
            popupRect.y + padY + headerBlockH,
            popupRect.w - (needsScroll ? scrollbarW : 0.0f),
            keel::max(10.0f, popupRect.h - padY * 2.0f - headerBlockH)
        );
        const keel::f32 maxScrollY = keel::max(0.0f, itemsTotalH - contentRect.h);
        keel::f32& scrollY = persist.scrollYByDepth[pIdx];
        scrollY = keel::clamp(scrollY, 0.0f, maxScrollY);

        const Id activeHighlightId = (pIdx < persist.highlightPath.size()) ? persist.highlightPath[pIdx] : Id::Null();
        const Id expandedChildId = (pIdx + 1 < persist.openPath.size()) ? persist.openPath[pIdx + 1] : Id::Null();

        DrawRow* arenaRows = ctx.GetArena().create_array<DrawRow>(rec.items.size());
        keel::SmallArray<Rect, 16> rectsForPanel;
        rectsForPanel.reserve(rec.items.size());

        PrevMenuPanel prevSnap{};
        prevSnap.panelId = rec.panelId;
        prevSnap.popupRect = popupRect;
        prevSnap.contentRect = contentRect;
        prevSnap.scrollY = scrollY;
        prevSnap.maxScrollY = maxScrollY;
        prevSnap.items.reserve(rec.items.size());

        keel::f32 curY = contentRect.y - scrollY;
        for (keel::usize i = 0; i < rec.items.size(); ++i) {
            const auto& src = rec.items[i];
            Rect r{};
            if (src.isSeparator) {
                r = Rect(popupRect.x + padX * 0.75f, keel::round(curY + sepH * 0.5f), popupRect.w - padX * 1.5f, 1.0f);
                curY += sepH;
            } else if (src.isSection) {
                r = Rect(popupRect.x + padX, curY, contentRect.w - padX * 2.0f, sectionH);
                curY += sectionH;
            } else {
                r = Rect(popupRect.x + 4.0f * s, curY, contentRect.w - 8.0f * s, rowH);
                curY += rowH;
            }
            rectsForPanel.push_back(r);

            const bool isSubExpanded = src.isSubMenu && expandedChildId.IsValid() && (src.id == expandedChildId);
            const bool isHighlighted = !src.isSeparator && !src.isSection && !src.disabled &&
                                       (src.id == activeHighlightId || isSubExpanded);

            arenaRows[i] = DrawRow{
                .id = src.id,
                .label = src.label,
                .shortcut = src.shortcut,
                .icon = src.icon,
                .variant = src.variant,
                .checked = src.checked,
                .hasCheck = src.hasCheck,
                .isRadio = src.isRadio,
                .disabled = src.disabled,
                .isSeparator = src.isSeparator,
                .isSection = src.isSection,
                .isSubMenu = src.isSubMenu,
                .hovered = isHighlighted,
                .subExpanded = isSubExpanded,
                .rect = r
            };

            PrevMenuItemHit hit{};
            hit.id = src.id;
            hit.rect = r;
            hit.firstAsciiLower = src.label.empty() ? char{0} : ToLowerAscii(src.label[0]);
            hit.isSubMenu = src.isSubMenu;
            hit.disabled = src.disabled;
            hit.nonSelectable = src.isSeparator || src.isSection;
            hit.keepOpen = src.keepOpen;
            hit.flatIndex = keel::as<keel::i32>(i);
            prevSnap.items.push_back(keel::move(hit));
        }

        DrawPanel dp{};
        dp.popupRect = popupRect;
        dp.contentRect = contentRect;
        dp.title = rec.title;
        dp.titleH = titleH;
        dp.titleSepY = hasTitle ? keel::round(popupRect.y + padY + titleH + titleSepH * 0.5f) : 0.0f;
        dp.scrollY = scrollY;
        dp.maxScrollY = maxScrollY;
        dp.contentTotalH = itemsTotalH;
        dp.anyLeadingSlot = anyLeadingSlot;
        dp.anyTrailingCheck = anyTrailingCheck;
        dp.anySubMenu = anySubMenu;
        dp.rows = keel::ArrayRef<DrawRow>(arenaRows, rec.items.size());

        computedPanels.push_back(dp);
        panelItemRects.push_back(keel::move(rectsForPanel));
        persist.prevPanels.push_back(keel::move(prevSnap));
    }

    // Register all open cascading panels (plus invisible 4dp seam bridges) with Context
    // so occlusion, outside-click & dirty-rects cover the entire tree cleanly
    ctx.OpenPopup(sess.rootId, computedPanels[0].popupRect, persist.anchorRect);
    for (keel::usize pIdx = 1; pIdx < computedPanels.size(); ++pIdx) {
        const Rect& childR = computedPanels[pIdx].popupRect;
        ctx.AddOpenPopupRect(childR);
        const Rect& parentR = computedPanels[sess.panels[pIdx].parentPanelIdx].popupRect;
        const keel::f32 topY = keel::max(parentR.y, childR.y);
        const keel::f32 botY = keel::min(parentR.Bottom(), childR.Bottom());
        if (botY > topY) {
            if (childR.x > parentR.Right()) {
                ctx.AddOpenPopupRect(Rect(parentR.Right(), topY, childR.x - parentR.Right(), botY - topY));
            } else if (parentR.x > childR.Right()) {
                ctx.AddOpenPopupRect(Rect(childR.Right(), topY, parentR.x - childR.Right(), botY - topY));
            }
        }
    }

    keel::u64 visualHash = detail::HashArgs(computedPanels.size(), activeSize);
    for (keel::usize pIdx = 0; pIdx < computedPanels.size(); ++pIdx) {
        const auto& dp = computedPanels[pIdx];
        const Id hl = (pIdx < persist.highlightPath.size()) ? persist.highlightPath[pIdx] : Id::Null();
        visualHash = detail::HashArgs(visualHash, dp.popupRect, dp.scrollY, dp.rows.size(), hl.value, dp.title);
    }
    ctx.SetOpenPopupVisualHash(visualHash);

    DrawPanel* arenaPanels = ctx.GetArena().create_array<DrawPanel>(computedPanels.size());
    for (keel::usize i = 0; i < computedPanels.size(); ++i) {
        arenaPanels[i] = computedPanels[i];
    }
    const keel::ArrayRef<DrawPanel> drawPanels(arenaPanels, computedPanels.size());

    const keel::f32 radius = opts.border.radius.has_value()
        ? *opts.border.radius
        : theme.ResolveRadius(keel::kNullOpt, activeSize);

    ctx.RegisterOverlay([drawPanels, radius, padX, padY, icSz, iconSlotW, submenuSlotW, labelFont, shortcutFont, sectionFont, titleFont, theme, opts](ICanvas& c) {
        const auto& pal = theme.palette;
        const bool isDark = (theme.mode == ThemeMode::Dark);
        const Color defaultSurfaceBg = (theme.controlStyle.frameChrome == FrameChrome::Bevel3D)
            ? pal.surfaceElevated.WithAlpha(255u)
            : (isDark ? pal.surfaceElevated.WithAlpha(250u) : pal.surfaceElevated.WithAlpha(252u));
        const Color surfaceBg = opts.background.IsBackgroundEnabled(true)
            ? (!opts.background.HasAny()
                ? defaultSurfaceBg
                : (opts.background.IsSurface()
                    ? defaultSurfaceBg.ScaleAlpha(opts.background.alphaScale)
                    : theme.ResolveColor(opts.background, false, false, false, defaultSurfaceBg)))
            : Color::Transparent();
        const keel::f32 borderW = (opts.border.width > 0.0f) ? opts.border.width : 0.0f;
        const Color borderCol = (borderW > 0.0f)
            ? theme.ResolveColor(opts.border.color, false, false, false, pal.borderHover)
            : Color::Transparent();

        for (const auto& panel : drawPanels) {
            const Rect& pr = panel.popupRect;

            theme.Painter().DrawFrame(c, theme, FrameKind::Popup, FramePaintParams{
                .rect = pr,
                .radius = radius,
                .explicitRadius = opts.border.radius.has_value(),
                .bg = surfaceBg,
                .border = borderCol,
                .borderWidth = borderW,
                .elevation = Elevation::Lg,
                .shadowScale = s
            });

            c.PushClip(pr);

            if (!panel.title.empty()) {
                const Rect titleRect(pr.x + padX, pr.y + padY, keel::max(10.0f, pr.w - padX * 2.0f), panel.titleH);
                c.DrawTextBoxStyled(panel.title, titleRect, titleFont, pal.textSecondary, FontStyle::Bold, 0.0f, 0.5f);
                theme.Painter().DrawDivider(
                    c, theme,
                    Vec2(pr.x + padX * 0.75f, panel.titleSepY),
                    Vec2(pr.Right() - padX * 0.75f, panel.titleSepY),
                    pal.border, 1.0f
                );
            }

            c.PushClip(panel.contentRect);
            for (const auto& it : panel.rows) {
                if (it.rect.Bottom() < panel.contentRect.y - 2.0f || it.rect.y > panel.contentRect.Bottom() + 2.0f) {
                    continue;
                }
                if (it.isSeparator) {
                    theme.Painter().DrawDivider(c, theme, Vec2(it.rect.x, it.rect.y), Vec2(it.rect.Right(), it.rect.y), pal.border, 1.0f);
                    continue;
                }
                if (it.isSection) {
                    c.DrawTextBoxStyled(it.label, it.rect, sectionFont, pal.textMuted, FontStyle::Bold, 0.0f, 0.5f);
                    continue;
                }

                const bool isDanger = (it.variant == UiVariant::Danger);
                const Color dangerCol = theme.GetVariant(UiVariant::Danger).bg;
                const bool iconWithCheck = it.icon.IsValid() && (it.hasCheck || it.checked);
                const keel::f32 itemRadius = keel::max(0.0f, radius - 2.0f * s);
                const bool accentHighlight = theme.Painter().DrawMenuItemHighlight(
                    c, theme, it.rect, itemRadius,
                    it.hovered,
                    it.checked && iconWithCheck,
                    it.subExpanded,
                    isDanger,
                    it.disabled,
                    dangerCol
                );

                Color fg = accentHighlight
                    ? pal.accentText
                    : (isDanger ? dangerCol : (it.checked ? pal.accent : pal.textPrimary));
                Color mutedFg = accentHighlight ? pal.accentText : pal.textSecondary;
                if (it.disabled) {
                    fg = fg.WithAlphaF(0.4f);
                    mutedFg = mutedFg.WithAlphaF(0.4f);
                }

                keel::f32 textX = it.rect.x + padX - 2.0f * s;
                if (panel.anyLeadingSlot) {
                    const Rect icRect(
                        textX + (iconSlotW - icSz) * 0.5f - 2.0f * s,
                        it.rect.y + (it.rect.h - icSz) * 0.5f,
                        icSz,
                        icSz
                    );
                    if (it.icon.IsValid()) {
                        const Color icCol = accentHighlight ? pal.accentText : (isDanger ? fg : (it.checked ? pal.accent : (it.hovered ? fg : mutedFg)));
                        it.icon.Draw(c, icRect, icCol);
                    } else if (it.checked) {
                        const Color markCol = accentHighlight ? pal.accentText : (isDanger ? dangerCol : pal.accent);
                        if (it.isRadio) {
                            const keel::f32 dotD = keel::max(6.0f, icSz * 0.46f);
                            const Rect dotRect(
                                icRect.x + (icRect.w - dotD) * 0.5f,
                                icRect.y + (icRect.h - dotD) * 0.5f,
                                dotD,
                                dotD
                            );
                            c.DrawRoundRect(dotRect, dotD * 0.5f, markCol);
                        } else {
                            Icon::Check().Draw(c, icRect, markCol);
                        }
                    }
                    textX += iconSlotW;
                }

                keel::f32 rightPad = it.rect.Right() - padX + 2.0f * s;
                if (panel.anySubMenu) {
                    if (it.isSubMenu) {
                        const keel::f32 chevSz = keel::max(10.0f, icSz * 0.80f);
                        const Rect chevRect(
                            rightPad - chevSz,
                            it.rect.y + (it.rect.h - chevSz) * 0.5f,
                            chevSz,
                            chevSz
                        );
                        Icon::ChevronRight().Draw(c, chevRect, it.hovered ? fg : mutedFg);
                    }
                    rightPad -= submenuSlotW;
                }
                if (panel.anyTrailingCheck) {
                    if (it.checked && iconWithCheck) {
                        const Rect chkRect(
                            rightPad - icSz,
                            it.rect.y + (it.rect.h - icSz) * 0.5f,
                            icSz,
                            icSz
                        );
                        const Color markCol = accentHighlight ? pal.accentText : (isDanger ? dangerCol : pal.accent);
                        if (it.isRadio) {
                            const keel::f32 dotD = keel::max(6.0f, icSz * 0.46f);
                            c.DrawRoundRect(
                                Rect(chkRect.x + (chkRect.w - dotD) * 0.5f, chkRect.y + (chkRect.h - dotD) * 0.5f, dotD, dotD),
                                dotD * 0.5f,
                                markCol
                            );
                        } else {
                            Icon::Check().Draw(c, chkRect, markCol);
                        }
                    }
                    rightPad -= iconSlotW;
                }

                keel::f32 availW = keel::max(10.0f, rightPad - textX);
                if (!it.shortcut.empty()) {
                    const keel::f32 sw = c.MeasureText(it.shortcut, shortcutFont).x;
                    c.DrawTextBox(
                        it.shortcut,
                        Rect(rightPad - sw - 2.0f * s, it.rect.y, sw + 4.0f * s, it.rect.h),
                        shortcutFont,
                        mutedFg,
                        1.0f,
                        0.5f
                    );
                    availW = keel::max(10.0f, rightPad - sw - 12.0f * s - textX);
                }

                c.DrawTextBox(
                    it.label,
                    Rect(textX, it.rect.y, availW, it.rect.h),
                    labelFont,
                    fg,
                    0.0f,
                    0.5f
                );
            }
            c.PopClip();

            // Subtle scrollbar thumb when menu overflows max height
            if (panel.maxScrollY > 0.5f && panel.contentTotalH > 0.0f) {
                const keel::f32 trackH = keel::max(16.0f, panel.contentRect.h - 4.0f);
                const keel::f32 thumbH = keel::clamp(trackH * (panel.contentRect.h / panel.contentTotalH), 18.0f, trackH);
                const keel::f32 ratio = panel.scrollY / panel.maxScrollY;
                const keel::f32 thumbY = panel.contentRect.y + 2.0f + ratio * (trackH - thumbH);
                const Rect thumbRect(pr.Right() - 5.5f * s, thumbY, 3.5f * s, thumbH);
                c.DrawRoundRect(thumbRect, 1.75f * s, pal.textMuted.WithAlphaF(0.45f));
            }

            c.PopClip();
        }
    });
}

} // namespace

// ============================================================================
// MenuScope Implementation
// ============================================================================

MenuScope::~MenuScope() noexcept {
    FinalizeIfNeeded();
}

MenuScope::MenuScope(MenuScope&& other) noexcept
    : m_session(other.m_session)
    , m_panelIdx(other.m_panelIdx)
    , m_isRoot(other.m_isRoot)
    , m_active(other.m_active) {
    other.m_session = nullptr;
    other.m_active = false;
    other.m_isRoot = false;
}

MenuScope& MenuScope::operator=(MenuScope&& other) noexcept {
    if (this != &other) {
        FinalizeIfNeeded();
        m_session = other.m_session;
        m_panelIdx = other.m_panelIdx;
        m_isRoot = other.m_isRoot;
        m_active = other.m_active;
        other.m_session = nullptr;
        other.m_active = false;
        other.m_isRoot = false;
    }
    return *this;
}

void MenuScope::FinalizeIfNeeded() noexcept {
    if (m_session != nullptr && m_isRoot && m_active) {
        FinalizeMenuSession(*m_session);
    }
    m_session = nullptr;
    m_active = false;
    m_isRoot = false;
}

Response MenuScope::Item(UiText label, const MenuItemOptions& opts, keel::SourceLocation /*loc*/) {
    Response res{};
    if (!m_session || !m_active || m_panelIdx >= m_session->panels.size()) {
        return res;
    }
    Context& ctx = *m_session->ctx;
    auto& panel = m_session->panels[m_panelIdx];
    const keel::u32 ordinal = m_session->itemOrdinals[m_panelIdx]++;
    const Id itemId = MakeMenuItemId(panel.panelId, ordinal, label.text);

    RecordedMenuItem rec{};
    rec.id = itemId;
    rec.label = ctx.AllocString(label.text);
    rec.shortcut = ctx.AllocString(opts.shortcut.text);
    rec.icon = ctx.AllocIcon(opts.icon);
    rec.variant = opts.variant;
    rec.checked = opts.checked;
    rec.hasCheck = opts.hasCheck || opts.checked;
    rec.isRadio = opts.isRadio;
    rec.disabled = opts.disabled;
    rec.keepOpen = opts.keepOpen;
    panel.items.push_back(rec);

    res.id = itemId;
    res.ctx = &ctx;
    if (!opts.disabled && m_session->activatedItemId.IsValid() && m_session->activatedItemId == itemId) {
        res.clicked = true;
        res.changed = true;
        m_session->anyActivated = true;
        if (!opts.keepOpen) {
            m_session->closeRequested = true;
        }
    }
    return res;
}

Response MenuScope::Checkbox(UiText label, bool& value, MenuItemOptions opts, keel::SourceLocation loc) {
    opts.hasCheck = true;
    opts.isRadio = false;
    opts.checked = value;
    Response r = Item(label, opts, loc);
    if (r.clicked) {
        value = !value;
        r.changed = true;
    }
    return r;
}

MenuScope MenuScope::SubMenu(UiText label, const MenuItemOptions& opts, keel::SourceLocation /*loc*/) {
    if (!m_session || !m_active || m_panelIdx >= m_session->panels.size()) {
        return MenuScope{};
    }
    Context& ctx = *m_session->ctx;
    auto& parentPanel = m_session->panels[m_panelIdx];
    const keel::u32 ordinal = m_session->itemOrdinals[m_panelIdx]++;
    const Id subId = MakeMenuItemId(parentPanel.panelId, ordinal, label.text);
    const keel::u32 parentItemIdx = keel::as<keel::u32>(parentPanel.items.size());

    RecordedMenuItem rec{};
    rec.id = subId;
    rec.label = ctx.AllocString(label.text);
    rec.shortcut = ctx.AllocString(opts.shortcut.text);
    rec.icon = ctx.AllocIcon(opts.icon);
    rec.variant = opts.variant;
    rec.disabled = opts.disabled;
    rec.isSubMenu = true;

    const keel::u32 childDepth = parentPanel.depth + 1;
    bool expanded = false;
    keel::u32 childPanelIdx = 0;

    if (!opts.disabled &&
        childDepth < kMaxMenuDepth &&
        childDepth < m_session->persist->openPath.size() &&
        m_session->persist->openPath[childDepth] == subId &&
        m_session->panels.size() < kMaxMenuDepth) {
        expanded = true;
        childPanelIdx = keel::as<keel::u32>(m_session->panels.size());
        rec.childPanelIdx = childPanelIdx;
    }

    parentPanel.items.push_back(rec);

    if (expanded) {
        RecordedMenuPanel childPanel{};
        childPanel.panelId = subId;
        childPanel.depth = childDepth;
        childPanel.parentPanelIdx = m_panelIdx;
        childPanel.parentItemIdx = parentItemIdx;
        m_session->panels.push_back(keel::move(childPanel));
        m_session->itemOrdinals.push_back(0);
        return MenuScope(m_session, childPanelIdx, false, true);
    }

    return MenuScope{};
}

void MenuScope::Section(UiText label, keel::SourceLocation /*loc*/) {
    if (!m_session || !m_active || m_panelIdx >= m_session->panels.size() || label.empty()) {
        return;
    }
    Context& ctx = *m_session->ctx;
    auto& panel = m_session->panels[m_panelIdx];
    const keel::u32 ordinal = m_session->itemOrdinals[m_panelIdx]++;
    RecordedMenuItem rec{};
    rec.id = MakeMenuItemId(panel.panelId, ordinal, label.text);
    rec.label = ctx.AllocString(label.text);
    rec.isSection = true;
    panel.items.push_back(rec);
}

void MenuScope::Separator(keel::SourceLocation /*loc*/) {
    if (!m_session || !m_active || m_panelIdx >= m_session->panels.size()) {
        return;
    }
    auto& panel = m_session->panels[m_panelIdx];
    const keel::u32 ordinal = m_session->itemOrdinals[m_panelIdx]++;
    RecordedMenuItem rec{};
    rec.id = MakeMenuItemId(panel.panelId, ordinal, "---");
    rec.isSeparator = true;
    panel.items.push_back(rec);
}

void MenuScope::Close() noexcept {
    if (m_session != nullptr) {
        m_session->closeRequested = true;
    }
}

keel::i32 MenuScope::GetHorizontalNavRequest() const noexcept {
    return m_session != nullptr ? m_session->horizontalNavRequest : 0;
}

bool MenuScope::AnyActivated() const noexcept {
    return m_session != nullptr && m_session->anyActivated;
}

// ============================================================================
// ContextMenu Entry Points
// ============================================================================

MenuScope detail::ContextMenu::OpenWithId(
    Context& ctx,
    Id rootId,
    bool& open,
    const ContextMenuOptions& opts,
    keel::i32* outHorizontalNav
) {
    if (outHorizontalNav != nullptr) {
        *outHorizontalNav = 0;
    }
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    auto& persist = ctx.GetOrCreateState<MenuTreePersistState>(rootId);

    Rect anchorRect = opts.anchorRect;
    if (anchorRect.w > 0.0f || anchorRect.h > 0.0f) {
        persist.anchorRect = anchorRect;
    } else if (!opts.anchorPos.has_value() && (persist.anchorRect.w > 0.0f || persist.anchorRect.h > 0.0f)) {
        anchorRect = persist.anchorRect;
    }

    const bool wasPopupOpen = ctx.IsPopupOpen(rootId);

    if (effectiveDisabled || !open) {
        if (wasPopupOpen) {
            ctx.ClosePopup();
        }
        open = false;
        ResetPersistState(persist);
        return MenuScope{};
    }

    if (!wasPopupOpen && persist.wasOpen) {
        // Dismissed by outside click in BeginFrame
        open = false;
        ResetPersistState(persist);
        return MenuScope{};
    }

    const Vec2 mouse = ctx.GetMousePos();
    const keel::f64 nowSec = ctx.GetCurrentTime();
    bool callerMovedAnchor = false;

    if (!wasPopupOpen) {
        // Fresh open: latch anchor position
        persist.rootAnchorPos = opts.anchorPos.value_or(
            (anchorRect.w > 0.0f || anchorRect.h > 0.0f)
                ? Vec2{anchorRect.x, anchorRect.Bottom() + 4.0f}
                : mouse
        );
        persist.lastCallerAnchorPos = opts.anchorPos;
        persist.openPath.clear();
        persist.openPath.push_back(rootId);
        persist.highlightPath.clear();
        persist.highlightPath.push_back(Id::Null());
        persist.scrollYByDepth.clear();
        persist.scrollYByDepth.push_back(0.0f);
        persist.prevPanels.clear();
        persist.submenuGraceUntilSec = 0.0;
        persist.lastMousePos = mouse;
        persist.wasOpen = true;
    } else if (opts.anchorPos.has_value() &&
               persist.lastCallerAnchorPos.has_value() &&
               *opts.anchorPos != *persist.lastCallerAnchorPos) {
        // Caller explicitly repositioned menu (e.g. RightClick elsewhere or Readout 1-click menu switch)
        persist.rootAnchorPos = *opts.anchorPos;
        persist.lastCallerAnchorPos = opts.anchorPos;
        persist.openPath.clear();
        persist.openPath.push_back(rootId);
        persist.highlightPath.clear();
        persist.highlightPath.push_back(Id::Null());
        persist.scrollYByDepth.clear();
        persist.scrollYByDepth.push_back(0.0f);
        persist.prevPanels.clear();
        persist.submenuGraceUntilSec = 0.0;
        callerMovedAnchor = true;
    }

    if (persist.openPath.empty()) {
        persist.openPath.push_back(rootId);
    }
    while (persist.highlightPath.size() < persist.openPath.size()) {
        persist.highlightPath.push_back(Id::Null());
    }
    while (persist.scrollYByDepth.size() < persist.openPath.size()) {
        persist.scrollYByDepth.push_back(0.0f);
    }

    auto* sess = ctx.GetArena().create<detail::MenuTreeFrameSession>();
    sess->ctx = &ctx;
    sess->rootId = rootId;
    sess->openPtr = &open;
    sess->persist = &persist;
    sess->opts = opts;
    sess->outHorizontalNav = outHorizontalNav;
    sess->callerMovedAnchor = callerMovedAnchor;

    // Auto-highlight first enabled item when freshly opened via keyboard (root or newly expanded submenu)
    if (opts.autoFocusFirst && !persist.prevPanels.empty() && !persist.highlightPath[0].IsValid()) {
        const keel::i32 first = FindFirstSelectable(persist.prevPanels[0]);
        if (first >= 0) {
            persist.highlightPath[0] = persist.prevPanels[0].items[keel::as<keel::usize>(first)].id;
        }
    }
    if (persist.pendingFocusDeepest && !persist.prevPanels.empty()) {
        const keel::usize deepest = persist.prevPanels.size() - 1;
        if (deepest < persist.highlightPath.size() && !persist.highlightPath[deepest].IsValid()) {
            const keel::i32 first = FindFirstSelectable(persist.prevPanels[deepest]);
            if (first >= 0) {
                persist.highlightPath[deepest] = persist.prevPanels[deepest].items[keel::as<keel::usize>(first)].id;
            }
        }
        persist.pendingFocusDeepest = false;
    }

    // ------------------------------------------------------------------------
    // Process pointer hover, safe-triangle intent, wheel scroll, clicks, and keyboard
    // against previous frame's panel layout snapshot
    // ------------------------------------------------------------------------
    if (wasPopupOpen && !callerMovedAnchor && !persist.prevPanels.empty()) {
        auto hitScope = ctx.ScopedOverlayHitCheck(rootId, OverlayLayer::Popup);

        // 1. Find deepest open panel containing pointer
        keel::i32 hoveredPanelDepth = -1;
        keel::i32 hoveredItemIdx = -1;
        for (keel::usize d = persist.prevPanels.size(); d > 0; --d) {
            const keel::usize depth = d - 1;
            const auto& panel = persist.prevPanels[depth];
            if (panel.popupRect.Contains(mouse)) {
                hoveredPanelDepth = keel::as<keel::i32>(depth);
                if (HalfOpenContains(panel.contentRect, mouse)) {
                    for (keel::usize i = 0; i < panel.items.size(); ++i) {
                        const auto& it = panel.items[i];
                        if (!it.nonSelectable && !it.disabled && HalfOpenContains(it.rect, mouse)) {
                            hoveredItemIdx = keel::as<keel::i32>(i);
                            break;
                        }
                    }
                }
                break;
            }
        }

        // 2. Mouse wheel scrolling inside hovered panel
        if (hoveredPanelDepth >= 0) {
            const keel::usize d = keel::as<keel::usize>(hoveredPanelDepth);
            const Vec2 wheel = ctx.GetMouseWheel();
            if (wheel.y != 0.0f && d < persist.prevPanels.size() && persist.prevPanels[d].maxScrollY > 0.5f) {
                if (d < persist.scrollYByDepth.size()) {
                    persist.scrollYByDepth[d] = keel::clamp(
                        persist.scrollYByDepth[d] - wheel.y * 36.0f,
                        0.0f,
                        persist.prevPanels[d].maxScrollY
                    );
                    ctx.ConsumeMouseWheel();
                    ctx.InvalidateAll();
                }
            }
        }

        // 3. Pointer hover & cascading submenu expansion (with safe-triangle intent grace)
        const bool mouseMoved = (mouse != persist.lastMousePos);
        if (hoveredPanelDepth >= 0 && hoveredItemIdx >= 0) {
            const keel::usize d = keel::as<keel::usize>(hoveredPanelDepth);
            const auto& hitItem = persist.prevPanels[d].items[keel::as<keel::usize>(hoveredItemIdx)];

            bool blockedBySafeTriangle = false;
            if (d + 1 < persist.prevPanels.size() && d + 1 < persist.openPath.size() &&
                hitItem.id != persist.openPath[d + 1] && mouseMoved) {
                const Rect& childRect = persist.prevPanels[d + 1].popupRect;
                const bool childOnRight = childRect.Center().x >= persist.prevPanels[d].popupRect.Center().x;
                const keel::f32 dx = mouse.x - persist.lastMousePos.x;
                const bool movingTowardChild = childOnRight ? (dx > 0.5f) : (dx < -0.5f);
                if (movingTowardChild) {
                    const Vec2 edgeTop = childOnRight ? Vec2(childRect.x, childRect.y) : Vec2(childRect.Right(), childRect.y);
                    const Vec2 edgeBot = childOnRight ? Vec2(childRect.x, childRect.Bottom()) : Vec2(childRect.Right(), childRect.Bottom());
                    if (nowSec > persist.submenuGraceUntilSec || persist.submenuGraceDepth != d) {
                        persist.submenuSafeOrigin = persist.lastMousePos;
                        persist.submenuGraceUntilSec = nowSec + 0.22;
                        persist.submenuGraceDepth = keel::as<keel::u32>(d);
                    }
                    if (PointInTriangle(mouse, persist.submenuSafeOrigin, edgeTop, edgeBot)) {
                        blockedBySafeTriangle = true;
                        ctx.RequestRepaint(2);
                    }
                }
            }

            if (!blockedBySafeTriangle) {
                if (d < persist.highlightPath.size()) {
                    persist.highlightPath[d] = hitItem.id;
                }
                if (hitItem.isSubMenu && d + 1 < kMaxMenuDepth) {
                    const bool alreadyOpen = (d + 1 < persist.openPath.size() && persist.openPath[d + 1] == hitItem.id);
                    if (!alreadyOpen) {
                        TruncateOpenPathToDepth(persist, d + 1);
                        persist.openPath.push_back(hitItem.id);
                        persist.highlightPath.push_back(Id::Null());
                        persist.scrollYByDepth.push_back(0.0f);
                        ctx.InvalidateAll();
                    }
                } else {
                    if (persist.openPath.size() > d + 1) {
                        TruncateOpenPathToDepth(persist, d + 1);
                        ctx.InvalidateAll();
                    }
                }
                persist.submenuGraceUntilSec = 0.0;
            }
        } else if (hoveredPanelDepth >= 0 && mouseMoved) {
            // Hovered over separator/section/padding inside panel d: clear highlight at panel d unless child submenu is open
            const keel::usize d = keel::as<keel::usize>(hoveredPanelDepth);
            if (persist.openPath.size() == d + 1 && d < persist.highlightPath.size()) {
                persist.highlightPath[d] = Id::Null();
            }
        }
        persist.lastMousePos = mouse;

        // 4. Keyboard navigation (operates on deepest open panel)
        const keel::usize activeDepth = persist.prevPanels.size() - 1;
        const auto& activePanel = persist.prevPanels[activeDepth];
        const Id curHl = (activeDepth < persist.highlightPath.size()) ? persist.highlightPath[activeDepth] : Id::Null();

        if (ctx.ConsumeKey(Key::Down)) {
            const keel::i32 nextIdx = StepSelectable(activePanel, curHl, 1);
            if (nextIdx >= 0) {
                const Id nextId = activePanel.items[keel::as<keel::usize>(nextIdx)].id;
                persist.highlightPath[activeDepth] = nextId;
                EnsureHighlightVisible(persist, activeDepth, activePanel, nextId);
                ctx.InvalidateAll();
            }
        } else if (ctx.ConsumeKey(Key::Up)) {
            const keel::i32 prevIdx = StepSelectable(activePanel, curHl, -1);
            if (prevIdx >= 0) {
                const Id prevId = activePanel.items[keel::as<keel::usize>(prevIdx)].id;
                persist.highlightPath[activeDepth] = prevId;
                EnsureHighlightVisible(persist, activeDepth, activePanel, prevId);
                ctx.InvalidateAll();
            }
        } else if (ctx.ConsumeKey(Key::Home)) {
            const keel::i32 firstIdx = FindFirstSelectable(activePanel);
            if (firstIdx >= 0) {
                const Id firstId = activePanel.items[keel::as<keel::usize>(firstIdx)].id;
                persist.highlightPath[activeDepth] = firstId;
                EnsureHighlightVisible(persist, activeDepth, activePanel, firstId);
                ctx.InvalidateAll();
            }
        } else if (ctx.ConsumeKey(Key::End)) {
            const keel::i32 lastIdx = FindLastSelectable(activePanel);
            if (lastIdx >= 0) {
                const Id lastId = activePanel.items[keel::as<keel::usize>(lastIdx)].id;
                persist.highlightPath[activeDepth] = lastId;
                EnsureHighlightVisible(persist, activeDepth, activePanel, lastId);
                ctx.InvalidateAll();
            }
        } else if (ctx.ConsumeKey(Key::Right)) {
            const keel::i32 curIdx = FindPrevItemIndex(activePanel, curHl);
            if (curIdx >= 0 && activePanel.items[keel::as<keel::usize>(curIdx)].isSubMenu &&
                !activePanel.items[keel::as<keel::usize>(curIdx)].disabled && activeDepth + 1 < kMaxMenuDepth) {
                const Id subId = activePanel.items[keel::as<keel::usize>(curIdx)].id;
                TruncateOpenPathToDepth(persist, activeDepth + 1);
                persist.openPath.push_back(subId);
                persist.highlightPath.push_back(Id::Null());
                persist.scrollYByDepth.push_back(0.0f);
                persist.pendingFocusDeepest = true;
                ctx.InvalidateAll();
            } else {
                sess->horizontalNavRequest = 1;
            }
        } else if (ctx.ConsumeKey(Key::Left)) {
            if (activeDepth > 0) {
                TruncateOpenPathToDepth(persist, activeDepth);
                ctx.InvalidateAll();
            } else {
                sess->horizontalNavRequest = -1;
            }
        } else if (ctx.ConsumeKey(Key::Escape)) {
            if (activeDepth > 0) {
                TruncateOpenPathToDepth(persist, activeDepth);
                ctx.InvalidateAll();
            } else {
                sess->closeRequested = true;
            }
        } else if (ctx.ConsumeKey(Key::Enter) || ctx.ConsumeKey(Key::Space)) {
            const keel::i32 curIdx = FindPrevItemIndex(activePanel, curHl);
            if (curIdx >= 0) {
                const auto& act = activePanel.items[keel::as<keel::usize>(curIdx)];
                if (!act.nonSelectable && !act.disabled) {
                    if (act.isSubMenu && activeDepth + 1 < kMaxMenuDepth) {
                        TruncateOpenPathToDepth(persist, activeDepth + 1);
                        persist.openPath.push_back(act.id);
                        persist.highlightPath.push_back(Id::Null());
                        persist.scrollYByDepth.push_back(0.0f);
                        persist.pendingFocusDeepest = true;
                        ctx.InvalidateAll();
                    } else {
                        sess->activatedItemId = act.id;
                        sess->activatedKeepOpen = act.keepOpen;
                        ctx.InvalidateAll();
                    }
                }
            }
        } else {
            // Type-ahead first-letter jump inside deepest active menu panel
            const auto typed = ctx.ConsumePendingTextInput();
            if (!typed.empty() && !typed[0].empty()) {
                const char ch = ToLowerAscii(typed[0][0]);
                if (ch >= 33 && ch <= 126) {
                    const keel::i32 n = keel::as<keel::i32>(activePanel.items.size());
                    const keel::i32 start = FindPrevItemIndex(activePanel, curHl);
                    for (keel::i32 step = 1; step <= n; ++step) {
                        const keel::i32 idx = (start < 0) ? (step - 1) : ((start + step) % n);
                        const auto& it = activePanel.items[keel::as<keel::usize>(idx)];
                        if (!it.nonSelectable && !it.disabled && it.firstAsciiLower == ch) {
                            persist.highlightPath[activeDepth] = it.id;
                            EnsureHighlightVisible(persist, activeDepth, activePanel, it.id);
                            ctx.InvalidateAll();
                            break;
                        }
                    }
                }
            }
        }

        // 5. Mouse click handling
        if (ctx.IsMouseClicked() || ctx.IsRightMouseClicked()) {
            const bool insideAnyPopup = (hoveredPanelDepth >= 0);
            const bool insideAnchor = (persist.anchorRect.w > 0.0f && persist.anchorRect.h > 0.0f) &&
                                      persist.anchorRect.Contains(mouse);
            if (insideAnyPopup) {
                if (ctx.IsMouseClicked() && hoveredItemIdx >= 0) {
                    const auto& clicked = persist.prevPanels[keel::as<keel::usize>(hoveredPanelDepth)]
                                              .items[keel::as<keel::usize>(hoveredItemIdx)];
                    if (!clicked.nonSelectable && !clicked.disabled && !clicked.isSubMenu) {
                        sess->activatedItemId = clicked.id;
                        sess->activatedKeepOpen = clicked.keepOpen;
                    }
                }
                ctx.ConsumeMouseClick();
                ctx.InvalidateAll();
            } else if (ctx.IsMouseClicked() && insideAnchor) {
                // Let anchor trigger handle toggle/switch cleanly
            } else {
                sess->closeRequested = true;
                ctx.ConsumeMouseClick();
                ctx.InvalidateAll();
            }
        }
    } else if (callerMovedAnchor && (ctx.IsMouseClicked() || ctx.IsRightMouseClicked())) {
        ctx.ConsumeMouseClick();
        ctx.InvalidateAll();
    }

    // Create root panel recorder
    RecordedMenuPanel rootPanel{};
    rootPanel.panelId = rootId;
    rootPanel.depth = 0;
    rootPanel.title = opts.title.empty() ? keel::StringRef{} : ctx.AllocString(opts.title.text);
    sess->panels.push_back(keel::move(rootPanel));
    sess->itemOrdinals.push_back(0);

    return MenuScope(sess, 0, true, true);
}

void detail::ContextMenu::ResetState(Context& ctx, Id rootId) noexcept {
    if (!rootId.IsValid()) return;
    if (auto* st = ctx.FindMutableState<MenuTreePersistState>(rootId)) {
        ResetPersistState(*st);
    }
}

MenuScope detail::ContextMenu::Open(
    Context& ctx,
    bool& open,
    const ContextMenuOptions& opts,
    keel::SourceLocation loc
) {
    const Id rootId = ctx.MakeChildId(loc);
    return OpenWithId(ctx, rootId, open, opts, nullptr);
}

ContextMenuResponse detail::ContextMenu::Run(
    Context& ctx,
    bool& open,
    keel::ArrayRef<ContextMenuItem> items,
    const ContextMenuOptions& opts,
    keel::SourceLocation loc
) {
    const Id rootId = ctx.MakeChildId(loc);
    ContextMenuResponse res{};
    res.id = rootId;
    res.ctx = &ctx;
    res.rect = opts.anchorRect;

    if (items.empty()) {
        if (ctx.IsPopupOpen(rootId)) {
            ctx.ClosePopup();
        }
        open = false;
        res.isOpen = false;
        return res;
    }

    keel::i32 horizNav = 0;
    {
        MenuScope root = OpenWithId(ctx, rootId, open, opts, &horizNav);
        if (root) {
            auto emitItems = [&](auto&& self, MenuScope& scope, keel::ArrayRef<ContextMenuItem> list, bool isRootLevel) -> void {
                for (keel::usize i = 0; i < list.size(); ++i) {
                    const auto& item = list[i];
                    if (item.isSeparatorOnly) {
                        scope.Separator(loc);
                        continue;
                    }
                    if (item.isSectionHeader) {
                        scope.Section(item.label, loc);
                        continue;
                    }
                    MenuItemOptions mo{};
                    mo.shortcut = item.shortcut;
                    mo.icon = item.icon;
                    mo.variant = item.variant;
                    mo.checked = item.checked;
                    mo.hasCheck = item.hasCheck;
                    mo.isRadio = item.isRadio;
                    mo.disabled = item.disabled;
                    mo.keepOpen = item.keepOpenOnClick;

                    if (!item.children.empty()) {
                        if (auto sub = scope.SubMenu(item.label, mo, loc)) {
                            self(self, sub, item.children, false);
                        }
                    } else {
                        const Response r = scope.Item(item.label, mo, loc);
                        if (r.clicked) {
                            res.clicked = true;
                            res.changed = true;
                            if (isRootLevel) {
                                res.clickedIndex = keel::as<keel::i32>(i);
                            }
                            res.clickedId = item.id;
                        }
                    }

                    if (item.separatorAfter) {
                        scope.Separator(loc);
                    }
                }
            };
            emitItems(emitItems, root, items, true);
        }
    }

    res.isOpen = open;
    res.horizontalNavRequest = horizNav;
    if (open) {
        const Rect popupR = ctx.GetOpenPopupRect();
        if (popupR.w > 0.0f && popupR.h > 0.0f) {
            res.rect = popupR;
        }
    }
    return res;
}

} // namespace guikit
