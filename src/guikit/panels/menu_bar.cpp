#include "guikit/panels/menu_bar.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/layout/spacer.hpp"
#include "guikit/panels/toolbar.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

struct MenuBarPersistState {
    keel::i32 openIndex{-1};
    keel::i32 totalMenusPrev{0};
    bool openedByKeyboard{false};
    bool openSlotActiveFlag{false};
    Vec2 lastMousePos{-1e6f, -1e6f};
    keel::SmallArray<Rect, 16> prevTriggerRects{};
    keel::SmallArray<bool, 16> prevTriggerDisabled{};
};

struct MenuBarTriggerData {
    keel::StringRef label{};
    bool hovered{false};
    bool open{false};
    bool disabled{false};
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
    m.radius = keel::round(5.0f * f);
    const keel::f32 vPad = keel::max(2.0f, keel::round((m.barHeight - m.itemHeight) * 0.5f));
    m.barPadding = Padding(keel::round(6.0f * f), vPad, keel::round(6.0f * f), vPad);
    return m;
}

void RenderMenuBarSurface(const Node* node, ICanvas& canvas, const UiTheme& /*theme*/) {
    if (!node) return;
    const Rect& r = node->computedRect;
    if (r.w <= 0.0f || r.h <= 0.0f) return;

    if (node->backgroundColor.a > 0) {
        if (node->cornerRadius > 0.0f) {
            canvas.DrawRoundRect(r, node->cornerRadius, node->backgroundColor, Color::Transparent(), 0.0f);
        } else {
            canvas.DrawRect(r, node->backgroundColor);
        }
    }

    // Header / Compact styles draw a clean 1px bottom border; Surface draws full border
    if (node->borderWidth > 0.0f && node->borderColor.a > 0) {
        if (node->cornerRadius > 0.0f) {
            canvas.DrawRoundRect(r, node->cornerRadius, Color::Transparent(), node->borderColor, node->borderWidth);
        } else {
            canvas.DrawLine(
                Vec2(r.x, r.Bottom() - 0.5f),
                Vec2(r.Right(), r.Bottom() - 0.5f),
                node->borderColor,
                node->borderWidth
            );
        }
    }
}

void RenderMenuBarTrigger(const Node* node, ICanvas& canvas, const UiTheme& theme) {
    const auto* data = node ? node->As<MenuBarTriggerData>() : nullptr;
    if (!data) return;

    const Rect& r = node->computedRect;
    if (r.w <= 0.0f || r.h <= 0.0f) return;

    const auto& pal = theme.palette;
    const bool isDark = (theme.mode == ThemeMode::Dark);

    Color bg = Color::Transparent();
    Color fg = pal.textPrimary;

    if (data->disabled) {
        fg = fg.WithAlphaF(0.4f);
    } else if (data->open) {
        bg = isDark ? pal.surfaceActive : pal.accent.WithAlphaF(0.14f);
        fg = pal.accent;
    } else if (data->hovered) {
        bg = pal.surfaceHover;
    }

    if (bg.a > 0) {
        canvas.DrawRoundRect(r, data->radius, bg);
    }
    canvas.DrawTextBox(data->label, r, data->fontSize, fg, 0.5f, 0.5f);
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
    canvas.DrawLine(Vec2(x, y0), Vec2(x, y0 + lineH), theme.palette.border, 1.0f);
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
    , m_horizNavRequest(other.m_horizNavRequest)
    , m_activeMenuIdx(other.m_activeMenuIdx)
    , m_hadOpenMenuAtStart(other.m_hadOpenMenuAtStart)
    , m_triggerClicked(other.m_triggerClicked)
    , m_openedScopeThisFrame(other.m_openedScopeThisFrame)
    , m_openSlotClosedByChild(other.m_openSlotClosedByChild)
    , m_activeBar(other.m_activeBar) {
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
        m_horizNavRequest = other.m_horizNavRequest;
        m_activeMenuIdx = other.m_activeMenuIdx;
        m_hadOpenMenuAtStart = other.m_hadOpenMenuAtStart;
        m_triggerClicked = other.m_triggerClicked;
        m_openedScopeThisFrame = other.m_openedScopeThisFrame;
        m_openSlotClosedByChild = other.m_openSlotClosedByChild;
        m_activeBar = other.m_activeBar;
        other.m_activeBar = false;
    }
    return *this;
}

void MenuBarScope::FinishBarIfNeeded() noexcept {
    if (m_activeBar && m_ctx != nullptr) {
        Context& ctx = *m_ctx;
        auto& st = ctx.GetOrCreateState<MenuBarPersistState>(m_barId);
        st.totalMenusPrev = m_menuCount;
        while (keel::as<keel::i32>(st.prevTriggerRects.size()) > m_menuCount) {
            st.prevTriggerRects.pop_back();
        }
        while (keel::as<keel::i32>(st.prevTriggerDisabled.size()) > m_menuCount) {
            st.prevTriggerDisabled.pop_back();
        }
        st.lastMousePos = ctx.GetMousePos();

        if (m_menuCount <= 0) {
            st.openIndex = -1;
            st.openedByKeyboard = false;
        } else if (m_horizNavRequest != 0 && st.openIndex >= 0) {
            st.openIndex = (st.openIndex + m_horizNavRequest + m_menuCount) % m_menuCount;
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
    if (keel::as<keel::usize>(idx) >= st.prevTriggerRects.size()) {
        st.prevTriggerRects.push_back(prevTriggerRect);
        st.prevTriggerDisabled.push_back(effDisabled);
    } else {
        st.prevTriggerRects[keel::as<keel::usize>(idx)] = prevTriggerRect;
        st.prevTriggerDisabled[keel::as<keel::usize>(idx)] = effDisabled;
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
    const TriggerResult tr = RunMenuTrigger(label, menuOpts.disabled, loc);
    if (!m_ctx || !m_activeBar) {
        return MenuScope{};
    }
    if (!tr.isOpen) {
        ContextMenu::ResetState(*m_ctx, tr.popupId);
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
    MenuScope scope = ContextMenu::OpenWithId(ctx, tr.popupId, st.openSlotActiveFlag, menuOpts, &m_horizNavRequest);
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
    const TriggerResult tr = RunMenuTrigger(label, menuOpts.disabled, loc);
    if (!m_ctx || !m_activeBar) {
        return res;
    }
    if (!tr.isOpen) {
        ContextMenu::ResetState(*m_ctx, tr.popupId);
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
        MenuScope root = ContextMenu::OpenWithId(ctx, tr.popupId, openFlag, menuOpts, &horizNav);
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
    guikit::Spacer::Run(*m_ctx, 1.0f, loc);
}

void MenuBarScope::Separator(keel::f32 heightRatio, keel::SourceLocation loc) {
    if (!m_ctx || !m_activeBar) return;
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

MenuBarScope MenuBar::Open(
    Context& ctx,
    MenuBarOptions opts,
    keel::SourceLocation loc
) {
    const Id barId = ctx.MakeChildId(loc);
    const bool effDisabled = opts.disabled || ctx.IsDisabled();

    Node* node = ctx.CreateNode(NodeType::MenuBar, barId, effDisabled);
    const UiTheme& theme = node->GetTheme(ctx.GetTheme());
    const MenuBarMetrics m = ComputeMenuBarMetrics(opts, theme);

    node->direction = FlexDirection::Row;
    node->align = Alignment::Center;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Fixed;
    node->fixedHeight = m.barHeight;
    node->gap = opts.gap.value_or(2.0f);
    node->padding = opts.padding.value_or(m.barPadding);
    node->uiSize = opts.size;

    const auto& pal = theme.palette;
    const bool isSurface = (opts.style == MenuBarStyle::Surface);
    const Color defaultBg = isSurface
        ? pal.surface
        : (theme.mode == ThemeMode::Dark
            ? Color::Lerp(pal.background, pal.surface, 0.75f)
            : Color::Lerp(pal.surface, pal.surfaceHover, 0.35f));

    node->backgroundColor = opts.background.IsBackgroundEnabled(true)
        ? theme.ResolveColor(opts.background, false, false, effDisabled, defaultBg)
        : Color::Transparent();
    node->cornerRadius = opts.border.radius.has_value()
        ? *opts.border.radius
        : (isSurface ? theme.ResolveRadius(keel::kNullOpt, opts.size) : 0.0f);
    node->borderWidth = (opts.border.width > 0.0f) ? opts.border.width : 1.0f;
    node->borderColor = theme.ResolveColor(opts.border.color, false, false, effDisabled, pal.border);
    node->renderFn = &RenderMenuBarSurface;
    node->visualHash = detail::HashArgs(opts.style, opts.size, opts.background, opts.border, effDisabled);

    ctx.FinishContainer(node);

    // Pre-resolve trigger click and hover-scrubbing BEFORE menu #0 runs so two menus
    // can never both open and register overlays on the same left-to-right scrub frame.
    bool triggerClickedInOpen = false;
    if (!effDisabled) {
        auto& st = ctx.GetOrCreateState<MenuBarPersistState>(barId);
        const Rect prevBarRect = ctx.GetPrevFrameBounds(barId);
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
    return scope;
}

} // namespace guikit
