#include "guikit/surfaces/menu_bar.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/style/style_painter.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/layout/spacer.hpp"
#include "guikit/surfaces/toolbar.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

struct MenuBarPersistState {
    keel::i32 openIndex{-1};
    keel::i32 totalMenusPrev{0};
    keel::i32 visibleMenusPrev{0};
    keel::f32 trailingReservePrev{0.0f};
    bool openedByKeyboard{false};
    bool openSlotActiveFlag{false};
    bool hasOverflowPrev{false};
    Vec2 lastMousePos{-1e6f, -1e6f};
    keel::SmallArray<Rect, 16> prevTriggerRects{};
    keel::SmallArray<bool, 16> prevTriggerDisabled{};
    keel::SmallArray<keel::f32, 16> prevMenuWidths{};
};

struct MenuBarTriggerData {
    keel::StringRef label{};
    bool hovered{false};
    bool open{false};
    bool disabled{false};
    bool isOverflowChevron{false};
    keel::f32 fontSize{13.0f};
    keel::f32 padX{10.0f};
    keel::f32 radius{5.0f};
};

struct MenuBarMetrics {
    keel::f32 barHeight{30.0f};
    keel::f32 itemHeight{24.0f};
    keel::f32 fontSize{13.0f};
    keel::f32 padX{10.0f};
    keel::f32 radius{5.0f};
    Padding barPadding{6.0f, 3.0f, 6.0f, 3.0f};
};

MenuBarMetrics ComputeMenuBarMetrics(const MenuBarOptions& opts, const UiTheme& theme) noexcept {
    MenuBarMetrics m{};
    const keel::f32 f = theme.SizeFactor(opts.size);
    const bool compact = (opts.style == MenuBarStyle::Compact);
    m.fontSize = keel::round(theme.ResolveFontSize(UiText{}, TextRole::LabelLarge, opts.size));
    m.itemHeight = keel::round((compact ? 22.0f : 24.0f) * f);
    m.barHeight = keel::round((compact ? 26.0f : 30.0f) * f);
    m.padX = keel::round((compact ? 8.0f : 10.0f) * f);
    m.radius = theme.ResolveRadius(keel::kNullOpt, opts.size);
    const keel::f32 vPad = keel::max(2.0f, keel::round((m.barHeight - m.itemHeight) * 0.5f));
    m.barPadding = Padding(keel::round(6.0f * f), vPad, keel::round(6.0f * f), vPad);
    return m;
}

void RenderMenuBarTrigger(const Node* node, ICanvas& canvas, const UiTheme& theme) {
    const auto* data = node ? node->As<MenuBarTriggerData>() : nullptr;
    if (!data) return;

    const Rect& r = node->computedRect;
    if (r.w <= 0.0f || r.h <= 0.0f) return;

    Color fg = theme.palette.textPrimary;
    Vec2 shift{};
    theme.Painter().DrawMenuBarTrigger(canvas, theme, r, data->radius, data->hovered, data->open, data->disabled, fg, shift);
    const Node* barNode = node->parent;
    if (barNode && barNode->type == NodeType::MenuBar &&
        barNode->backgroundColor.a == 0u &&
        barNode->parent && barNode->parent->type == NodeType::HeaderBar &&
        !data->open) {
        const HeaderBarChromeMetrics hdrMetrics = theme.Painter().GetHeaderBarMetrics(theme, barNode->parent->active);
        if (hdrMetrics.accentCaption) {
            fg = data->disabled ? hdrMetrics.titleColor.WithAlphaF(theme.disabledOpacity) : hdrMetrics.titleColor;
        }
    }
    const Rect textR(r.x + shift.x, r.y + shift.y, r.w, r.h);
    if (data->isOverflowChevron) {
        const keel::f32 cx = textR.Center().x;
        const keel::f32 cy = textR.Center().y;
        const keel::f32 sz = keel::clamp(data->fontSize * 0.25f, 2.5f, 4.0f);
        const keel::f32 sep = sz * 1.25f;
        const Vec2 l0{cx - sep * 0.5f - sz * 0.5f, cy - sz};
        const Vec2 l1{cx - sep * 0.5f + sz * 0.5f, cy};
        const Vec2 l2{cx - sep * 0.5f - sz * 0.5f, cy + sz};
        const Vec2 r0{cx + sep * 0.5f - sz * 0.5f, cy - sz};
        const Vec2 r1{cx + sep * 0.5f + sz * 0.5f, cy};
        const Vec2 r2{cx + sep * 0.5f - sz * 0.5f, cy + sz};
        canvas.DrawLine(l0, l1, fg, 1.35f);
        canvas.DrawLine(l1, l2, fg, 1.35f);
        canvas.DrawLine(r0, r1, fg, 1.35f);
        canvas.DrawLine(r1, r2, fg, 1.35f);
    } else {
        theme.Painter().DrawControlText(canvas, theme, data->label, textR, data->fontSize, fg, data->disabled, 0.5f, 0.5f);
    }
}

struct MenuBarSepPayload {
    keel::f32 heightRatio{0.55f};
};

void RenderMenuBarSeparator(const Node* node, ICanvas& canvas, const UiTheme& theme) {
    const auto* p = node ? node->As<MenuBarSepPayload>() : nullptr;
    const keel::f32 ratio = p ? p->heightRatio : 0.55f;
    const Rect& r = node->computedRect;
    const keel::f32 lineH = keel::max(8.0f, r.h * keel::clamp(ratio, 0.2f, 0.95f));
    const keel::f32 x = keel::round(r.x + r.w * 0.5f);
    const keel::f32 y0 = keel::round(r.y + (r.h - lineH) * 0.5f);
    theme.Painter().DrawDivider(canvas, theme, Vec2(x, y0), Vec2(x, y0 + lineH), theme.palette.border, 1.0f);
}

} // namespace

// ============================================================================
// MenuBarScope Implementation
// ============================================================================

MenuBarScope::MenuBarScope(
    Context* ctx,
    Node* node,
    Id barId,
    const MenuBarOptions& opts,
    Response res
) noexcept
    : ContainerScope(ctx, node)
    , response(res)
    , m_barId(barId)
    , m_opts(opts)
    , m_activeBar(node != nullptr) {
    if (ctx != nullptr && m_activeBar) {
        auto& st = ctx->GetOrCreateState<MenuBarPersistState>(barId);
        m_hadOpenMenuAtStart = (st.openIndex >= 0);
        m_activeMenuIdx = st.openIndex;
    }
}

MenuBarScope::~MenuBarScope() {
    FinishBarIfNeeded();
}

MenuBarScope::MenuBarScope(MenuBarScope&& other) noexcept
    : ContainerScope(keel::move(other))
    , response(other.response)
    , m_barId(other.m_barId)
    , m_opts(other.m_opts)
    , m_menuCount(other.m_menuCount)
    , m_visibleLimit(other.m_visibleLimit)
    , m_prevVisibleMenusSnapshot(other.m_prevVisibleMenusSnapshot)
    , m_horizNavRequest(other.m_horizNavRequest)
    , m_activeMenuIdx(other.m_activeMenuIdx)
    , m_hadOpenMenuAtStart(other.m_hadOpenMenuAtStart)
    , m_triggerClicked(other.m_triggerClicked)
    , m_openedScopeThisFrame(other.m_openedScopeThisFrame)
    , m_openSlotClosedByChild(other.m_openSlotClosedByChild)
    , m_overflowEmitted(other.m_overflowEmitted)
    , m_prevHasOverflowSnapshot(other.m_prevHasOverflowSnapshot)
    , m_seenSpacer(other.m_seenSpacer)
    , m_activeBar(other.m_activeBar)
    , m_overflowMenu(keel::move(other.m_overflowMenu)) {
    other.m_activeBar = false;
}

MenuBarScope& MenuBarScope::operator=(MenuBarScope&& other) noexcept {
    if (this != &other) {
        FinishBarIfNeeded();
        ContainerScope::operator=(keel::move(other));
        response = other.response;
        m_barId = other.m_barId;
        m_opts = other.m_opts;
        m_menuCount = other.m_menuCount;
        m_visibleLimit = other.m_visibleLimit;
        m_prevVisibleMenusSnapshot = other.m_prevVisibleMenusSnapshot;
        m_horizNavRequest = other.m_horizNavRequest;
        m_activeMenuIdx = other.m_activeMenuIdx;
        m_hadOpenMenuAtStart = other.m_hadOpenMenuAtStart;
        m_triggerClicked = other.m_triggerClicked;
        m_openedScopeThisFrame = other.m_openedScopeThisFrame;
        m_openSlotClosedByChild = other.m_openSlotClosedByChild;
        m_overflowEmitted = other.m_overflowEmitted;
        m_prevHasOverflowSnapshot = other.m_prevHasOverflowSnapshot;
        m_seenSpacer = other.m_seenSpacer;
        m_activeBar = other.m_activeBar;
        m_overflowMenu = keel::move(other.m_overflowMenu);
        other.m_activeBar = false;
    }
    return *this;
}

void MenuBarScope::FinishBarIfNeeded() noexcept {
    // Finalize overflow MenuScope FIRST so its session updates m_horizNavRequest & openSlotActiveFlag
    m_overflowMenu = MenuScope{};

    if (m_activeBar && m_ctx != nullptr) {
        Context& ctx = *m_ctx;
        auto& st = ctx.GetOrCreateState<MenuBarPersistState>(m_barId);

        // Measure trailing widgets placed after a Spacer() so next frame reserves room for them
        keel::f32 trailingReserve = 0.0f;
        if (m_node != nullptr && m_seenSpacer) {
            bool afterSpacer = false;
            const keel::f32 gap = m_node->gap;
            for (const Node* ch = m_node->firstChild; ch != nullptr; ch = ch->nextSibling) {
                if (ch->IsOutOfFlow()) continue;
                if (ch->isSpacer) {
                    afterSpacer = true;
                    continue;
                }
                if (afterSpacer) {
                    const Rect r = ctx.GetPrevUnclippedBounds(ch->id);
                    const keel::f32 w = (r.w > 0.0f) ? r.w : ((ch->widthMode == SizeMode::Fixed) ? ch->fixedWidth : ch->intrinsicSize.x);
                    if (w > 0.0f) {
                        trailingReserve += w + gap;
                    }
                }
            }
        }
        const keel::f32 prevTrailing = st.trailingReservePrev;
        st.trailingReservePrev = trailingReserve;
        if (keel::abs(trailingReserve - prevTrailing) > 0.5f) {
            ctx.InvalidateAll();
        }

        const keel::i32 visibleOnBar = keel::min(m_menuCount, m_visibleLimit);
        const keel::i32 barSlotCount = visibleOnBar + (m_overflowEmitted ? 1 : 0);

        st.totalMenusPrev = m_menuCount;
        st.visibleMenusPrev = visibleOnBar;
        st.hasOverflowPrev = m_overflowEmitted;
        while (keel::as<keel::i32>(st.prevTriggerRects.size()) > barSlotCount) {
            st.prevTriggerRects.pop_back();
        }
        while (keel::as<keel::i32>(st.prevTriggerDisabled.size()) > barSlotCount) {
            st.prevTriggerDisabled.pop_back();
        }
        while (keel::as<keel::i32>(st.prevMenuWidths.size()) > m_menuCount) {
            st.prevMenuWidths.pop_back();
        }
        st.lastMousePos = ctx.GetMousePos();

        if (visibleOnBar != m_prevVisibleMenusSnapshot || m_overflowEmitted != m_prevHasOverflowSnapshot) {
            ctx.InvalidateAll();
        }

        if (barSlotCount <= 0) {
            st.openIndex = -1;
            st.openedByKeyboard = false;
        } else if (m_horizNavRequest != 0 && st.openIndex >= 0) {
            st.openIndex = (st.openIndex + m_horizNavRequest + barSlotCount) % barSlotCount;
            st.openedByKeyboard = true;
            ctx.ClosePopup();
            ctx.InvalidateAll();
        } else if (m_openSlotClosedByChild || (m_openedScopeThisFrame && !st.openSlotActiveFlag)) {
            st.openIndex = -1;
            st.openedByKeyboard = false;
        } else if (m_hadOpenMenuAtStart && !m_triggerClicked && ctx.IsMouseClicked()) {
            const Rect barRect = ctx.GetPrevFrameBounds(m_barId);
            if (barRect.Contains(ctx.GetMousePos()) && !ctx.IsPointInOpenPopup(ctx.CurrentSpace().Apply(ctx.GetMousePos()))) {
                st.openIndex = -1;
                st.openedByKeyboard = false;
                ctx.ClosePopup();
                ctx.ConsumeMouseClick();
                ctx.InvalidateAll();
            }
        }
    }
    m_activeBar = false;
    Close();
}

void MenuBarScope::RecordOverflowMenuWidth(keel::i32 idx, UiText label) noexcept {
    if (!m_ctx || idx < 0) return;
    Context& ctx = *m_ctx;
    auto& st = ctx.GetOrCreateState<MenuBarPersistState>(m_barId);
    const MenuBarMetrics m = ComputeMenuBarMetrics(m_opts, ctx.GetTheme());
    while (keel::as<keel::i32>(st.prevMenuWidths.size()) <= idx) {
        st.prevMenuWidths.push_back(0.0f);
    }
    if (st.prevMenuWidths[keel::as<keel::usize>(idx)] <= 0.0f) {
        st.prevMenuWidths[keel::as<keel::usize>(idx)] =
            keel::ceil(TextUtil::MeasureUiTextWidth(label, m.fontSize, ctx.GetCanvas()) + m.padX * 2.0f);
    }
}

void MenuBarScope::EnsureOverflowSlot(const ContextMenuOptions& sampleOpts, keel::SourceLocation /*loc*/) {
    if (m_overflowEmitted || !m_ctx || !m_activeBar) {
        return;
    }
    m_overflowEmitted = true;

    Context& ctx = *m_ctx;
    auto& st = ctx.GetOrCreateState<MenuBarPersistState>(m_barId);
    const keel::i32 slotIdx = m_visibleLimit;
    const Id triggerId(detail::HashArgs(m_barId.value, keel::u64(0x4D424F5646545247ull)));
    const Id popupId(detail::HashArgs(m_barId.value, keel::u64(0x4D424F5646504F50ull)));

    const bool effDisabled = m_opts.disabled || ctx.IsDisabled();
    const UiTheme& theme = ctx.GetTheme();
    const MenuBarMetrics m = ComputeMenuBarMetrics(m_opts, theme);
    const keel::f32 overflowW = keel::max(24.0f, keel::ceil(m.itemHeight + 4.0f));

    const Rect prevTriggerRect = ctx.GetPrevFrameBounds(triggerId);
    while (keel::as<keel::i32>(st.prevTriggerRects.size()) <= slotIdx) {
        st.prevTriggerRects.push_back(Rect{});
        st.prevTriggerDisabled.push_back(false);
    }
    st.prevTriggerRects[keel::as<keel::usize>(slotIdx)] = prevTriggerRect;
    st.prevTriggerDisabled[keel::as<keel::usize>(slotIdx)] = effDisabled;

    const Vec2 mouse = ctx.GetMousePos();
    const bool pointerInOpenPopup = ctx.IsPointInOpenPopup(ctx.CurrentSpace().Apply(mouse));
    const bool hovered = !effDisabled &&
                         prevTriggerRect.w > 0.0f && prevTriggerRect.h > 0.0f &&
                         !pointerInOpenPopup &&
                         prevTriggerRect.Contains(mouse) &&
                         (ctx.IsRectHovered(triggerId, prevTriggerRect) || st.openIndex >= 0);

    if (hovered) {
        ctx.SetCursor(CursorType::Pointer);
    }

    const bool isOpen = !effDisabled && (st.openIndex == slotIdx);

    Node* trigNode = ctx.CreateNode(NodeType::Custom, triggerId, effDisabled);
    trigNode->uiSize = m_opts.size;
    trigNode->hovered = hovered;
    trigNode->widthMode = SizeMode::Fixed;
    trigNode->fixedWidth = overflowW;
    trigNode->heightMode = SizeMode::Fixed;
    trigNode->fixedHeight = m.itemHeight;

    auto* payload = ctx.AllocPayload<MenuBarTriggerData>();
    payload->label = ">>";
    payload->hovered = hovered;
    payload->open = isOpen;
    payload->disabled = effDisabled;
    payload->isOverflowChevron = true;
    payload->fontSize = m.fontSize;
    payload->padX = m.padX;
    payload->radius = m.radius;

    trigNode->text = payload->label;
    trigNode->SetPayload(payload);
    trigNode->renderFn = &RenderMenuBarTrigger;
    trigNode->visualHash = detail::HashArgs(keel::u8(1), hovered, isOpen, effDisabled, m.fontSize);

    if (!isOpen) {
        detail::ContextMenu::ResetState(ctx, popupId);
        return;
    }

    ContextMenuOptions ovOpts = sampleOpts;
    ovOpts.disabled = false;
    const Rect barRect = ctx.GetPrevFrameBounds(m_barId);
    ovOpts.anchorRect = (barRect.w > 0.0f && barRect.h > 0.0f) ? barRect : prevTriggerRect;
    if (!ovOpts.anchorPos.has_value() && prevTriggerRect.w > 0.0f && prevTriggerRect.h > 0.0f) {
        const keel::f32 anchorBottom = (barRect.h > 0.0f) ? barRect.Bottom() : prevTriggerRect.Bottom();
        ovOpts.anchorPos = Vec2(prevTriggerRect.x, anchorBottom + 3.0f);
    }
    if (st.openedByKeyboard) {
        ovOpts.autoFocusFirst = true;
    }

    st.openSlotActiveFlag = true;
    m_openedScopeThisFrame = true;
    m_overflowMenu = detail::ContextMenu::OpenWithId(ctx, popupId, st.openSlotActiveFlag, ovOpts, &m_horizNavRequest);
    if (!st.openSlotActiveFlag) {
        m_openSlotClosedByChild = true;
    }
}

MenuBarScope::TriggerResult MenuBarScope::RunMenuTrigger(
    UiText label,
    bool itemDisabled,
    keel::SourceLocation loc
) {
    TriggerResult out{};
    if (!m_ctx || !m_activeBar) {
        return out;
    }

    Context& ctx = *m_ctx;
    auto& st = ctx.GetOrCreateState<MenuBarPersistState>(m_barId);
    const keel::i32 idx = m_menuCount++;
    const Id triggerId = ctx.MakeChildId(loc);
    const Id popupId(detail::HashArgs(m_barId.value, keel::u64(0x4D42504F505550ull), idx));

    const bool effDisabled = m_opts.disabled || itemDisabled || ctx.IsDisabled();
    const UiTheme& theme = ctx.GetTheme();
    const MenuBarMetrics m = ComputeMenuBarMetrics(m_opts, theme);

    const Rect prevTriggerRect = ctx.GetPrevFrameBounds(triggerId);
    while (keel::as<keel::i32>(st.prevTriggerRects.size()) <= idx) {
        st.prevTriggerRects.push_back(Rect{});
        st.prevTriggerDisabled.push_back(false);
    }
    st.prevTriggerRects[keel::as<keel::usize>(idx)] = prevTriggerRect;
    st.prevTriggerDisabled[keel::as<keel::usize>(idx)] = effDisabled;

    while (keel::as<keel::i32>(st.prevMenuWidths.size()) <= idx) {
        st.prevMenuWidths.push_back(0.0f);
    }
    const Rect prevUnclipped = ctx.GetPrevUnclippedBounds(triggerId);
    if (prevUnclipped.w > 0.0f) {
        st.prevMenuWidths[keel::as<keel::usize>(idx)] = prevUnclipped.w;
    } else if (st.prevMenuWidths[keel::as<keel::usize>(idx)] <= 0.0f) {
        st.prevMenuWidths[keel::as<keel::usize>(idx)] =
            keel::ceil(TextUtil::MeasureUiTextWidth(label, m.fontSize, ctx.GetCanvas()) + m.padX * 2.0f);
    }

    const Vec2 mouse = ctx.GetMousePos();
    const bool pointerInOpenPopup = ctx.IsPointInOpenPopup(ctx.CurrentSpace().Apply(mouse));
    const bool hovered = !effDisabled &&
                         prevTriggerRect.w > 0.0f && prevTriggerRect.h > 0.0f &&
                         !pointerInOpenPopup &&
                         prevTriggerRect.Contains(mouse) &&
                         (ctx.IsRectHovered(triggerId, prevTriggerRect) || st.openIndex >= 0);

    if (hovered) {
        ctx.SetCursor(CursorType::Pointer);
    }

    const bool isOpen = !effDisabled && (st.openIndex == idx);

    // Create trigger leaf node
    Node* trigNode = ctx.CreateNode(NodeType::Custom, triggerId, effDisabled);
    trigNode->uiSize = m_opts.size;
    trigNode->hovered = hovered;
    trigNode->widthMode = SizeMode::Fit;
    trigNode->heightMode = SizeMode::Fixed;
    trigNode->fixedHeight = m.itemHeight;

    auto* payload = ctx.AllocPayload<MenuBarTriggerData>();
    payload->label = ctx.AllocString(label.text);
    payload->hovered = hovered;
    payload->open = isOpen;
    payload->disabled = effDisabled;
    payload->fontSize = m.fontSize;
    payload->padX = m.padX;
    payload->radius = m.radius;

    trigNode->text = payload->label;
    trigNode->SetPayload(payload);
    trigNode->measureFn = [](const Node* n, keel::f32 /*availW*/, ICanvas* canvas, const UiTheme& /*t*/) -> Vec2 {
        const auto* d = n->As<MenuBarTriggerData>();
        if (!d) return Vec2(44.0f, 24.0f);
        const keel::f32 tw = TextUtil::MeasureUiTextWidth(UiText(d->label), d->fontSize, canvas);
        return Vec2(keel::ceil(tw + d->padX * 2.0f), n->fixedHeight);
    };
    trigNode->renderFn = &RenderMenuBarTrigger;
    trigNode->visualHash = detail::HashArgs(payload->label, hovered, isOpen, effDisabled, m.fontSize);

    out.popupId = popupId;
    out.triggerRect = prevTriggerRect;
    out.isOpen = isOpen;
    return out;
}

MenuScope MenuBarScope::Menu(
    UiText label,
    ContextMenuOptions menuOpts,
    keel::SourceLocation loc
) {
    if (!m_ctx || !m_activeBar) {
        return MenuScope{};
    }

    if (m_menuCount >= m_visibleLimit) {
        Context& ctx = *m_ctx;
        const keel::i32 idx = m_menuCount++;
        RecordOverflowMenuWidth(idx, label);
        EnsureOverflowSlot(menuOpts, loc);
        if (!m_overflowMenu) {
            return MenuScope{};
        }
        MenuItemOptions subOpts{};
        subOpts.disabled = m_opts.disabled || menuOpts.disabled || ctx.IsDisabled();
        return m_overflowMenu.SubMenu(label, subOpts, loc);
    }

    const TriggerResult tr = RunMenuTrigger(label, menuOpts.disabled, loc);
    if (!tr.isOpen) {
        detail::ContextMenu::ResetState(*m_ctx, tr.popupId);
        return MenuScope{};
    }

    Context& ctx = *m_ctx;
    auto& st = ctx.GetOrCreateState<MenuBarPersistState>(m_barId);
    const Rect barRect = ctx.GetPrevFrameBounds(m_barId);

    menuOpts.anchorRect = (barRect.w > 0.0f && barRect.h > 0.0f) ? barRect : tr.triggerRect;
    if (!menuOpts.anchorPos.has_value() && tr.triggerRect.w > 0.0f && tr.triggerRect.h > 0.0f) {
        const keel::f32 anchorBottom = (barRect.h > 0.0f) ? barRect.Bottom() : tr.triggerRect.Bottom();
        menuOpts.anchorPos = Vec2(tr.triggerRect.x, anchorBottom + 3.0f);
    }
    if (st.openedByKeyboard) {
        menuOpts.autoFocusFirst = true;
    }

    st.openSlotActiveFlag = true;
    m_openedScopeThisFrame = true;
    MenuScope scope = detail::ContextMenu::OpenWithId(ctx, tr.popupId, st.openSlotActiveFlag, menuOpts, &m_horizNavRequest);
    if (!st.openSlotActiveFlag) {
        m_openSlotClosedByChild = true;
    }
    return scope;
}

ContextMenuResponse MenuBarScope::Menu(
    UiText label,
    keel::ArrayRef<ContextMenuItem> items,
    ContextMenuOptions menuOpts,
    keel::SourceLocation loc
) {
    ContextMenuResponse res{};
    if (!m_ctx || !m_activeBar) {
        return res;
    }

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

    if (m_menuCount >= m_visibleLimit) {
        Context& ctx = *m_ctx;
        const keel::i32 idx = m_menuCount++;
        RecordOverflowMenuWidth(idx, label);
        EnsureOverflowSlot(menuOpts, loc);
        if (m_overflowMenu) {
            MenuItemOptions subOpts{};
            subOpts.disabled = m_opts.disabled || menuOpts.disabled || ctx.IsDisabled();
            if (auto sub = m_overflowMenu.SubMenu(label, subOpts, loc)) {
                res.isOpen = true;
                emitItems(emitItems, sub, items, true);
            }
        }
        return res;
    }

    const TriggerResult tr = RunMenuTrigger(label, menuOpts.disabled, loc);
    if (!tr.isOpen) {
        detail::ContextMenu::ResetState(*m_ctx, tr.popupId);
        return res;
    }

    Context& ctx = *m_ctx;
    auto& st = ctx.GetOrCreateState<MenuBarPersistState>(m_barId);
    const Rect barRect = ctx.GetPrevFrameBounds(m_barId);

    menuOpts.anchorRect = (barRect.w > 0.0f && barRect.h > 0.0f) ? barRect : tr.triggerRect;
    if (!menuOpts.anchorPos.has_value() && tr.triggerRect.w > 0.0f && tr.triggerRect.h > 0.0f) {
        const keel::f32 anchorBottom = (barRect.h > 0.0f) ? barRect.Bottom() : tr.triggerRect.Bottom();
        menuOpts.anchorPos = Vec2(tr.triggerRect.x, anchorBottom + 3.0f);
    }
    if (st.openedByKeyboard) {
        menuOpts.autoFocusFirst = true;
    }

    bool openFlag = true;
    keel::i32 horizNav = 0;
    {
        MenuScope root = detail::ContextMenu::OpenWithId(ctx, tr.popupId, openFlag, menuOpts, &horizNav);
        if (root) {
            emitItems(emitItems, root, items, true);
        }
    }

    res.isOpen = openFlag;
    res.horizontalNavRequest = horizNav;
    if (horizNav != 0) {
        m_horizNavRequest = horizNav;
    } else if (!openFlag) {
        m_openSlotClosedByChild = true;
    }
    return res;
}

void MenuBarScope::Spacer(keel::SourceLocation loc) {
    if (!m_ctx || !m_activeBar) return;
    m_seenSpacer = true;
    guikit::detail::Spacer::Run(*m_ctx, 1.0f, loc);
}

void MenuBarScope::Separator(keel::f32 heightRatio, keel::SourceLocation loc) {
    if (!m_ctx || !m_activeBar) return;
    if (m_overflowEmitted && !m_seenSpacer) {
        if (m_overflowMenu) {
            m_overflowMenu.Separator(loc);
        }
        return;
    }
    Context& ctx = *m_ctx;
    const Id sepId = ctx.MakeChildId(loc);
    const MenuBarMetrics m = ComputeMenuBarMetrics(m_opts, ctx.GetTheme());
    Node* sep = ctx.CreateNode(NodeType::Custom, sepId);
    sep->widthMode = SizeMode::Fixed;
    sep->fixedWidth = 9.0f;
    sep->heightMode = SizeMode::Fixed;
    sep->fixedHeight = m.itemHeight;
    sep->SetPayload(ctx.AllocPayload<MenuBarSepPayload>(MenuBarSepPayload{.heightRatio = heightRatio}));
    sep->renderFn = &RenderMenuBarSeparator;
    sep->visualHash = detail::HashArgs(heightRatio);
}

bool MenuBarScope::IsAnyMenuOpen() const noexcept {
    if (!m_ctx || !m_activeBar) return false;
    const auto& st = m_ctx->GetOrCreateState<MenuBarPersistState>(m_barId);
    return st.openIndex >= 0;
}

void MenuBarScope::CloseMenu() noexcept {
    if (!m_ctx || !m_activeBar) return;
    auto& st = m_ctx->GetOrCreateState<MenuBarPersistState>(m_barId);
    if (st.openIndex >= 0) {
        st.openIndex = -1;
        st.openedByKeyboard = false;
        m_ctx->ClosePopup();
        m_ctx->InvalidateAll();
    }
}

// ============================================================================
// MenuBar Entry Point
// ============================================================================

MenuBarScope detail::MenuBar::Open(
    Context& ctx,
    MenuBarOptions opts,
    keel::SourceLocation loc
) {
    const Id barId = ctx.MakeChildId(loc);
    const bool effDisabled = opts.disabled || ctx.IsDisabled();

    Node* node = ctx.CreateNode(NodeType::MenuBar, barId, effDisabled);
    const UiTheme& theme = node->GetTheme(ctx.GetTheme());
    const MenuBarMetrics m = ComputeMenuBarMetrics(opts, theme);

    const bool inHeaderBar = (node->parent != nullptr && node->parent->type == NodeType::HeaderBar);

    node->direction = FlexDirection::Row;
    node->align = Alignment::Center;
    node->clipChildren = true;
    node->widthMode = inHeaderBar ? SizeMode::Fit : SizeMode::Stretch;
    node->heightMode = SizeMode::Fixed;
    node->fixedHeight = m.barHeight;
    node->gap = opts.gap.value_or(2.0f);
    node->padding = opts.padding.value_or(inHeaderBar ? Padding(2.0f, 0.0f, 2.0f, 0.0f) : m.barPadding);
    node->uiSize = opts.size;

    const auto& pal = theme.palette;
    const bool isSurface = (opts.style == MenuBarStyle::Surface);
    const Color defaultBg = inHeaderBar
        ? Color::Transparent()
        : (isSurface
            ? pal.surface
            : (theme.mode == ThemeMode::Dark
                ? Color::Lerp(pal.background, pal.surface, 0.75f)
                : Color::Lerp(pal.surface, pal.surfaceHover, 0.35f)));

    node->backgroundColor = opts.background.IsBackgroundEnabled(!inHeaderBar)
        ? theme.ResolveColor(opts.background, false, false, effDisabled, defaultBg)
        : Color::Transparent();
    const bool isFloatingShell = (theme.shellStyle == ShellStyle::Floating);
    node->cornerRadius = opts.border.radius.has_value()
        ? *opts.border.radius
        : (isSurface ? theme.ResolveRadius(keel::kNullOpt, opts.size) : theme.shell.panelRadius);
    node->explicitRadius = (opts.border.radius.has_value() || isSurface) ? 1 : 0;
    node->explicitBorder = opts.border.HasExplicitStroke() ? 1 : 0;
    node->borderWidth = (opts.border.width > 0.0f)
        ? opts.border.width
        : (inHeaderBar ? 0.0f : (isSurface ? 1.0f : theme.shell.panelBorderWidth));
    node->borderColor = theme.ResolveColor(opts.border.color, false, false, effDisabled, pal.border);
    ctx.ApplySurfaceGradients(
        node,
        theme,
        opts.background.IsBackgroundEnabled(!inHeaderBar) ? opts.background : StateColor{},
        (node->borderWidth > 0.0f) ? opts.border.color : StateColor{},
        false,
        false,
        effDisabled
    );
    node->SetSurfaceRole(inHeaderBar ? SurfaceRole::None : (isSurface ? SurfaceRole::Flat : SurfaceRole::DockEdge));
    node->SetDockSeam((inHeaderBar || isSurface || isFloatingShell) ? DockSeam::None : DockSeam::Bottom);
    node->visualHash = detail::HashArgs(
        keel::as<keel::u8>(theme.shellStyle),
        opts.style,
        opts.size,
        opts.background,
        opts.border,
        effDisabled
    );

    ctx.FinishContainer(node);

    // Compute how many top-level menus fit before collapsing the remainder into `»`
    auto& stInit = ctx.GetOrCreateState<MenuBarPersistState>(barId);
    const Rect prevBarRectInit = ctx.GetPrevFrameBounds(barId);
    keel::i32 visibleLimit = 1024;
    if (opts.overflow == MenuBarOverflow::Auto && !inHeaderBar &&
        prevBarRectInit.w > 1.0f && stInit.totalMenusPrev > 0 && !stInit.prevMenuWidths.empty()) {
        const keel::f32 availW = keel::max(0.0f, prevBarRectInit.w - node->padding.Horizontal() - stInit.trailingReservePrev);
        const keel::f32 gap = node->gap;
        const keel::f32 overflowW = keel::max(24.0f, keel::ceil(m.itemHeight + 4.0f));
        const keel::i32 count = keel::min(stInit.totalMenusPrev, keel::as<keel::i32>(stInit.prevMenuWidths.size()));

        keel::f32 sumAll = 0.0f;
        for (keel::i32 i = 0; i < count; ++i) {
            if (i > 0) sumAll += gap;
            sumAll += keel::max(24.0f, stInit.prevMenuWidths[keel::as<keel::usize>(i)]);
        }

        if (sumAll > availW + 0.5f) {
            keel::f32 usedW = 0.0f;
            keel::i32 fitCount = 0;
            for (keel::i32 i = 0; i < count; ++i) {
                const keel::f32 mw = keel::max(24.0f, stInit.prevMenuWidths[keel::as<keel::usize>(i)]);
                const keel::f32 nextUsed = usedW + (i > 0 ? gap : 0.0f) + mw;
                if (nextUsed + gap + overflowW <= availW + 0.5f) {
                    usedW = nextUsed;
                    fitCount = i + 1;
                } else {
                    break;
                }
            }
            visibleLimit = fitCount;
        }
    }

    const keel::i32 prevVisibleSnap = stInit.visibleMenusPrev;
    const bool prevHasOverflowSnap = stInit.hasOverflowPrev;
    if (stInit.totalMenusPrev > 0) {
        const keel::i32 predictedSlots = keel::min(stInit.totalMenusPrev, visibleLimit) +
                                         (visibleLimit < stInit.totalMenusPrev ? 1 : 0);
        if (stInit.openIndex >= predictedSlots) {
            stInit.openIndex = -1;
            stInit.openedByKeyboard = false;
            ctx.ClosePopup();
            ctx.InvalidateAll();
        }
    }

    // Pre-resolve trigger click and hover-scrubbing BEFORE menu #0 runs so two menus
    // can never both open and register overlays on the same left-to-right scrub frame.
    bool triggerClickedInOpen = false;
    if (!effDisabled) {
        auto& st = stInit;
        const Rect prevBarRect = prevBarRectInit;
        const Vec2 mouse = ctx.GetMousePos();
        const bool mouseMoved = (mouse != st.lastMousePos);
        const bool pointerInOpenPopup = ctx.IsPointInOpenPopup(ctx.CurrentSpace().Apply(mouse));
        const bool canInteractBar = !pointerInOpenPopup &&
                                    prevBarRect.w > 0.0f && prevBarRect.h > 0.0f &&
                                    prevBarRect.Contains(mouse) &&
                                    (ctx.IsRectHovered(barId, prevBarRect) || st.openIndex >= 0);
        if (canInteractBar) {
            for (keel::usize i = 0; i < st.prevTriggerRects.size(); ++i) {
                if (i < st.prevTriggerDisabled.size() && st.prevTriggerDisabled[i]) {
                    continue;
                }
                if (st.prevTriggerRects[i].Contains(mouse)) {
                    const keel::i32 idx = keel::as<keel::i32>(i);
                    ctx.SetCursor(CursorType::Pointer);
                    if (mouseMoved && st.openIndex >= 0 && st.openIndex != idx) {
                        st.openIndex = idx;
                        st.openedByKeyboard = false;
                        ctx.ClosePopup();
                        ctx.InvalidateAll();
                    }
                    if (ctx.IsMouseClicked()) {
                        triggerClickedInOpen = true;
                        st.openIndex = (st.openIndex == idx) ? -1 : idx;
                        st.openedByKeyboard = false;
                        ctx.ClosePopup();
                        ctx.ConsumeMouseClick();
                        ctx.InvalidateAll();
                    }
                    break;
                }
            }
        }
    } else {
        auto& st = ctx.GetOrCreateState<MenuBarPersistState>(barId);
        if (st.openIndex >= 0) {
            st.openIndex = -1;
            st.openedByKeyboard = false;
            ctx.ClosePopup();
        }
    }

    Response res{};
    res.id = barId;
    res.ctx = &ctx;
    res.rect = ctx.GetPrevFrameBounds(barId);
    MenuBarScope scope(&ctx, node, barId, opts, res);
    scope.m_triggerClicked = triggerClickedInOpen;
    scope.m_visibleLimit = visibleLimit;
    scope.m_prevVisibleMenusSnapshot = prevVisibleSnap;
    scope.m_prevHasOverflowSnapshot = prevHasOverflowSnap;
    return scope;
}

} // namespace guikit
