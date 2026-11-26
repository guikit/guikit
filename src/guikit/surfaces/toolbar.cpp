#include "guikit/surfaces/toolbar.hpp"
#include "guikit/display/separator.hpp"
#include "guikit/layout/spacer.hpp"
#include "guikit/overlays/context_menu.hpp"
#include "guikit/actions/button.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/solver.hpp"
#include "guikit/style/style_painter.hpp"
#include "guikit/surfaces/carousel.hpp"
#include "guikit/layout/scroll_area.hpp"
#include "keel/math.h"
#include "keel/small_array.h"

namespace guikit {

namespace {
struct ToolButtonPayload {
    ButtonOptions btn{};
    UiText origText{};
    UiText badge{};
    UiVariant badgeVariant{UiVariant::Primary};
    keel::f32 expandedWidth{0.0f};
    keel::f32 splitChevronWidth{0.0f};
    bool origSquare{false};
    bool isSplit{false};
    bool chevronHovered{false};
    bool chevronPressed{false};
    bool chevronOpen{false};
};

void RenderToolBadge(const Node* n, const ToolButtonPayload* p, ICanvas& canvas, const UiTheme& theme) noexcept {
    if (!p || p->badge.empty()) return;
    const auto& pal = theme.palette;
    const Color semanticBg = theme.GetSemanticAccent(p->badgeVariant);
    const Color resolvedBg = p->badge.color.HasAny()
        ? theme.ResolveSemanticAccent(p->badge.color, p->badgeVariant, n->hovered, n->pressed, false)
        : semanticBg;
    const keel::f32 badgeFont = keel::max(9.0f, theme.ResolveFontSize(p->badge, TextRole::LabelSmall, n->uiSize) - 1.5f);
    const Vec2 badgeDims = canvas.MeasureText(p->badge.text, badgeFont);
    const keel::f32 badgeH = keel::min(n->computedRect.h * 0.48f, keel::max(13.0f, badgeFont + 3.0f));
    const keel::f32 badgeW = keel::min(n->computedRect.w - 4.0f, keel::max(badgeH, badgeDims.x + 6.0f));
    const keel::f32 rightInset = p->isSplit ? (p->splitChevronWidth + 2.0f) : 2.0f;
    const Rect badgeRect(
        n->computedRect.Right() - badgeW - rightInset,
        n->computedRect.y + 2.0f,
        badgeW,
        badgeH
    );
    const Color badgeBg = n->disabled ? resolvedBg.WithAlphaF(theme.disabledOpacity) : resolvedBg;
    const Color baseInk = Color::BestTextColor(resolvedBg, Color::Hex(0x0F172AFF), Color::Hex(0xFFFFFFFF));
    const Color badgeTextCol = n->disabled ? baseInk.WithAlphaF(theme.disabledOpacity) : baseInk;
    theme.Painter().DrawBadge(canvas, theme, badgeRect, badgeBg, pal.surface, 1.5f, badgeH * 0.5f);
    canvas.DrawTextBox(p->badge.text, badgeRect, badgeFont, badgeTextCol, 0.5f, 0.5f);
}

void RenderToolButtonNode(const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
    const auto* p = n->As<ToolButtonPayload>();
    if (!p || !p->isSplit) {
        detail::Button::Render(canvas, n->computedRect, p ? p->btn : ButtonOptions{}, n->uiSize, n->hovered, n->pressed, n->disabled, theme, n->focused);
        RenderToolBadge(n, p, canvas, theme);
        return;
    }

    const Rect fullRect = n->computedRect;
    if (fullRect.w <= 0.0f || fullRect.h <= 0.0f) return;

    const keel::f32 chevW = keel::min(p->splitChevronWidth, fullRect.w * 0.45f);
    const Rect mainRect(fullRect.x, fullRect.y, keel::max(0.0f, fullRect.w - chevW), fullRect.h);
    const Rect chevRect(fullRect.Right() - chevW, fullRect.y, chevW, fullRect.h);

    const ComponentMetrics& m = theme.GetMetrics(n->uiSize);
    const auto& pal = theme.palette;
    const keel::f32 radius = p->btn.border.radius.value_or(m.cornerRadius);
    const bool active = p->btn.active;
    const bool mainHov = n->hovered;
    const bool mainPress = n->pressed;
    const bool chevHov = p->chevronHovered || p->chevronOpen;
    const bool chevPress = p->chevronPressed || p->chevronOpen;
    const bool anyHov = mainHov || chevHov;
    const bool anyPress = mainPress || chevPress;

    const detail::ButtonResolvedColors rc = detail::Button::ResolveColors(
        p->btn, theme, anyHov, anyPress, n->disabled, n->focused
    );
    const Color baseBg = n->disabled ? rc.restBg : (active ? rc.activeBg : rc.restBg);
    const Color mainBg = n->disabled ? baseBg : (mainPress ? rc.activeBg : (mainHov ? rc.hoverBg : baseBg));
    const Color chevBg = n->disabled ? baseBg : (chevPress ? rc.activeBg : (chevHov ? rc.hoverBg : baseBg));
    const Color textCol = rc.textCol;
    const Color borderCol = (rc.isGhost && !n->disabled && anyHov && !active && rc.borderW <= 0.0f)
        ? pal.borderSubtle
        : rc.borderCol;
    const keel::f32 borderW = (rc.isGhost && !n->disabled && anyHov && !active && rc.borderW <= 0.0f)
        ? 1.0f
        : rc.borderW;

    const FrameKind splitKind = rc.isGhost ? FrameKind::ToolbarButton : FrameKind::Button;
    const keel::Optional<Gradient> bgGrad = p->btn.background.IsBackgroundEnabled(true)
        ? theme.ResolveGradient(p->btn.background, anyHov, anyPress || active, n->disabled)
        : keel::kNullOpt;
    const keel::Optional<Gradient> bdrGrad = (borderW > 0.0f)
        ? theme.ResolveGradient(p->btn.border.color, anyHov, anyPress || active || n->focused, n->disabled)
        : keel::kNullOpt;
    theme.Painter().DrawFrame(canvas, theme, splitKind, FramePaintParams{
        .rect = fullRect,
        .radius = radius,
        .explicitRadius = p->btn.border.radius.has_value(),
        .bg = baseBg,
        .border = borderCol,
        .borderWidth = borderW,
        .explicitBorder = p->btn.border.HasExplicitStroke(),
        .hovered = anyHov,
        .pressed = anyPress,
        .active = active,
        .focused = n->focused && !n->disabled,
        .disabled = n->disabled,
        .accent = pal.accent,
        .bgGradient = bgGrad.has_value() ? &(*bgGrad) : nullptr,
        .borderGradient = bdrGrad.has_value() ? &(*bdrGrad) : nullptr
    });

    const keel::f32 frameInset = keel::max(borderW, theme.Painter().GetChromeMetrics(theme).frameBevelInset);
    const Rect innerRect(
        fullRect.x + frameInset,
        fullRect.y + frameInset,
        keel::max(0.0f, fullRect.w - frameInset * 2.0f),
        keel::max(0.0f, fullRect.h - frameInset * 2.0f)
    );
    const keel::f32 innerRadius = keel::max(0.0f, radius - frameInset);

    canvas.PushClip(fullRect);

    if (mainBg.a > 0 && mainBg != baseBg && mainRect.w > 0.0f && innerRect.w > 0.0f && innerRect.h > 0.0f) {
        canvas.PushClip(mainRect);
        canvas.DrawRoundRect(innerRect, innerRadius, mainBg, Color::Transparent(), 0.0f);
        canvas.PopClip();
    }
    if (chevBg.a > 0 && chevBg != baseBg && chevRect.w > 0.0f && innerRect.w > 0.0f && innerRect.h > 0.0f) {
        canvas.PushClip(chevRect);
        canvas.DrawRoundRect(innerRect, innerRadius, chevBg, Color::Transparent(), 0.0f);
        canvas.PopClip();
    }

    const keel::f32 mainShift = theme.Painter().GetButtonContentOffset(theme, mainPress, active, n->disabled);
    const Rect shiftedMainRect(mainRect.x + mainShift, mainRect.y + mainShift, mainRect.w, mainRect.h);
    ButtonOptions mainBtn = p->btn;
    mainBtn.style = ButtonStyle::Ghost;
    mainBtn.background = StateColor::None();
    mainBtn.border = {.width = 0.0f};
    mainBtn.active = false;
    mainBtn.text.color = StateColor{textCol, textCol, textCol, textCol};
    if (!mainBtn.iconColor.has_value()) {
        mainBtn.iconColor = textCol;
    }
    detail::Button::Render(canvas, shiftedMainRect, mainBtn, n->uiSize, false, false, n->disabled, theme, false);

    const keel::f32 sepX = keel::floor(chevRect.x) + 0.5f;
    const keel::f32 sepPadY = (anyHov && !active) ? 2.0f : keel::round(fullRect.h * 0.20f);
    const Color sepCol = active
        ? textCol.WithAlphaF(n->disabled ? 0.15f : 0.28f)
        : pal.border.WithAlphaF(n->disabled ? 0.20f : (anyHov ? 0.85f : 0.45f));
    theme.Painter().DrawDivider(canvas, theme, Vec2(sepX, fullRect.y + sepPadY), Vec2(sepX, fullRect.Bottom() - sepPadY), sepCol, 1.0f);

    const keel::f32 chevShift = theme.Painter().GetButtonContentOffset(theme, chevPress, false, n->disabled);
    const keel::f32 chevIconSz = keel::round(m.iconSize * 0.72f);
    const Rect chevIconRect(
        chevRect.x + (chevRect.w - chevIconSz) * 0.5f + chevShift,
        chevRect.y + (chevRect.h - chevIconSz) * 0.5f + chevShift,
        chevIconSz,
        chevIconSz
    );
    const Color chevCol = textCol.WithAlphaF(n->disabled ? theme.disabledOpacity : (chevHov || active ? 0.95f : 0.75f));
    Icon::ChevronDown().Draw(canvas, chevIconRect, chevCol);

    RenderToolBadge(n, p, canvas, theme);
    canvas.PopClip();
}
} // namespace

void ToolbarScope::InvalidateContext() noexcept {
    if (m_ctx) {
        m_ctx->InvalidateAll();
    }
}

void ToolbarScope::Separator(keel::f32 heightRatio, keel::SourceLocation loc) {
    if (!m_ctx || !m_node) return;
    guikit::detail::Separator::Run(*m_ctx, heightRatio, m_node->borderColor, loc);
}

void ToolbarScope::Spacer(keel::SourceLocation loc) {
    if (!m_ctx) return;
    const Id id = m_ctx->MakeChildId(loc);
    Node* spacer = m_ctx->CreateNode(NodeType::Spacer, id);
    if (m_opts.orientation == Orientation::Vertical) {
        spacer->widthMode = SizeMode::Fit;
        spacer->heightMode = SizeMode::Stretch;
    } else {
        spacer->widthMode = SizeMode::Stretch;
    }
    spacer->isSpacer = true;
    spacer->flexGrow = 1.0f;
    spacer->measureFn = [](const Node*, keel::f32 /*availableWidth*/, ICanvas*, const UiTheme&) noexcept -> Vec2 {
        return {0.0f, 0.0f};
    };
}

void ToolbarScope::Gap(keel::f32 sizeDp, keel::SourceLocation loc) {
    if (!m_ctx) return;
    guikit::detail::Spacer::Gap(*m_ctx, sizeDp, loc);
}

Response ToolbarScope::ToolButton(
    ToolButtonOptions opts,
    keel::SourceLocation loc
) {
    if (!m_ctx) return Response{};
    if (!opts.style.has_value()) {
        opts.style = m_opts.buttonStyle;
    }
    return detail::Toolbar::ToolButton(*m_ctx, opts, loc);
}

Response ToolbarScope::Toggle(
    bool& active,
    ToolButtonOptions opts,
    keel::SourceLocation loc
) {
    if (!m_ctx) return Response{};
    ApplyToggleStyle(opts, m_opts.buttonStyle, active);
    Response res = detail::Toolbar::ToolButton(*m_ctx, opts, loc);
    if (res.clicked) {
        active = !active;
        res.changed = true;
        m_ctx->InvalidateAll();
    }
    return res;
}

ContextMenuResponse ToolbarScope::Dropdown(
    ToolButtonOptions opts,
    keel::ArrayRef<ContextMenuItem> menuItems,
    ContextMenuOptions menuOpts,
    keel::SourceLocation loc
) {
    if (!m_ctx) return ContextMenuResponse{};
    if (!opts.style.has_value()) {
        opts.style = m_opts.buttonStyle;
    }
    const bool hasExplicitTrailing =
        opts.trailingIcon.IsValid() ||
        (opts.icon.IsValid() && opts.iconPlacement == Placement::Trailing);
    if (!opts.square && !hasExplicitTrailing) {
        opts.trailingIcon = Icon::ChevronDown();
    }
    struct DropdownMenuState {
        bool open{false};
        Id popupId{};
    };
    const Id rootId = m_ctx->MakeChildId(loc);
    auto& state = m_ctx->GetOrCreateState<DropdownMenuState>(rootId);
    const bool visualOpen = state.open && state.popupId.IsValid() && m_ctx->IsPopupOpen(state.popupId);
    opts.active = opts.active || visualOpen;

    auto scope = m_ctx->IdScope(rootId.value);
    const Response btnRes = detail::Toolbar::ToolButton(*m_ctx, opts, keel::SourceLocation::current());
    if (opts.disabled || m_ctx->IsDisabled()) {
        state.open = false;
    } else if (btnRes.clicked) {
        state.open = !state.open;
        m_ctx->InvalidateAll();
    }
    menuOpts.anchorRect = btnRes.rect;
    ContextMenuResponse res = detail::ContextMenu::Run(*m_ctx, state.open, menuItems, menuOpts, keel::SourceLocation::current());
    state.popupId = res.id;
    return res;
}

SplitButtonResponse ToolbarScope::SplitButton(
    ToolButtonOptions opts,
    keel::ArrayRef<ContextMenuItem> menuItems,
    ContextMenuOptions menuOpts,
    keel::SourceLocation loc
) {
    if (!m_ctx) return SplitButtonResponse{};
    if (!opts.style.has_value()) {
        opts.style = m_opts.buttonStyle;
    }
    return detail::Toolbar::SplitButton(*m_ctx, opts, menuItems, keel::move(menuOpts), loc);
}

SplitButtonResponse ToolbarScope::SplitToggle(
    bool& active,
    keel::i32& selectedIndex,
    keel::ArrayRef<keel::StringRef> items,
    ToolButtonOptions opts,
    ContextMenuOptions menuOpts,
    keel::SourceLocation loc
) {
    if (!m_ctx) return SplitButtonResponse{};
    keel::SmallArray<ContextMenuItem, 16> menuItems;
    menuItems.reserve(items.size());
    for (keel::usize i = 0; i < items.size(); ++i) {
        ContextMenuItem item{};
        item.label = UiText{items[i]};
        item.hasCheck = true;
        item.checked = (keel::as<keel::i32>(i) == selectedIndex);
        menuItems.push_back(item);
    }
    return SplitToggle(active, selectedIndex, keel::ArrayRef<ContextMenuItem>(menuItems), keel::move(opts), keel::move(menuOpts), loc);
}

SplitButtonResponse ToolbarScope::SplitToggle(
    bool& active,
    keel::i32& selectedIndex,
    keel::ArrayRef<ContextMenuItem> items,
    ToolButtonOptions opts,
    ContextMenuOptions menuOpts,
    keel::SourceLocation loc
) {
    if (!m_ctx) return SplitButtonResponse{};
    ApplyToggleStyle(opts, m_opts.buttonStyle, active);

    keel::SmallArray<ContextMenuItem, 16> menuItems;
    menuItems.reserve(items.size());
    for (keel::usize i = 0; i < items.size(); ++i) {
        ContextMenuItem item = items[i];
        if (!item.isSeparatorOnly) {
            item.hasCheck = true;
            item.checked = (keel::as<keel::i32>(i) == selectedIndex);
        }
        menuItems.push_back(item);
    }

    SplitButtonResponse res = detail::Toolbar::SplitButton(*m_ctx, opts, menuItems, keel::move(menuOpts), loc);
    if (res.clicked) {
        active = !active;
        res.toggled = true;
        res.changed = true;
        m_ctx->InvalidateAll();
    }
    if (res.selectionChanged && res.selectedIndex >= 0 && res.selectedIndex < keel::as<keel::i32>(menuItems.size())) {
        if (selectedIndex != res.selectedIndex) {
            selectedIndex = res.selectedIndex;
        }
        if (!active) {
            active = true;
            res.toggled = true;
        }
        res.changed = true;
        m_ctx->InvalidateAll();
    } else {
        res.selectedIndex = selectedIndex;
    }
    return res;
}

ToolbarScope detail::Toolbar::Open(
    Context& ctx,
    ToolbarOptions opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    Node* node = ctx.CreateNode(NodeType::Toolbar, id, opts.disabled);
    const UiTheme& theme = node->GetTheme(ctx.GetTheme());
    const keel::f32 density = node->GetDensity();
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = node->disabled;
    const ComponentMetrics& m = theme.GetMetrics(activeSize);
    const keel::f32 barExtent = m.defaultHeight + 8.0f;
    const bool isVertical = (opts.orientation == Orientation::Vertical);

    Response res{};
    res.id = id;
    res.rect = ctx.GetPrevUnclippedBounds(id);
    res.ctx = &ctx;

    node->direction = isVertical
        ? FlexDirection::Column
        : (opts.overflow == ToolbarOverflow::Wrap ? FlexDirection::Wrap : FlexDirection::Row);
    node->align = Alignment::Center;
    const keel::f32 defaultGap = keel::max(4.0f, keel::round(theme.GetGap(activeSize) * 0.5f)) * density;
    const keel::f32 crossPad = 4.0f * density;
    node->gap = opts.gap.has_value() ? theme.ResolveGap(opts.gap, activeSize, density) : defaultGap;
    node->runGap = node->gap;
    node->clipChildren = true;

    Padding toolbarPadding;
    if (opts.padding.has_value()) {
        if (opts.padding->IsToken()) {
            const UiSize t = opts.padding->GetToken();
            if (t.IsNone()) {
                toolbarPadding = Padding{};
            } else {
                const keel::f32 g = keel::max(4.0f, keel::round(theme.GetGap(t) * 0.5f)) * density;
                toolbarPadding = isVertical
                    ? Padding::Symmetric(crossPad, g)
                    : Padding::Symmetric(g, crossPad);
            }
        } else {
            toolbarPadding = theme.ResolvePadding(opts.padding, UiSize::None, density);
        }
    } else {
        toolbarPadding = isVertical
            ? Padding::Symmetric(crossPad, defaultGap)
            : Padding::Symmetric(defaultGap, crossPad);
    }
    node->padding = toolbarPadding;

    if (isVertical) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = barExtent;
        node->heightMode = (opts.style == ToolbarStyle::Floating) ? SizeMode::Fit : SizeMode::Stretch;
    } else if (opts.overflow == ToolbarOverflow::Wrap) {
        node->widthMode = (opts.style == ToolbarStyle::Floating) ? SizeMode::Fit : SizeMode::Stretch;
        node->heightMode = SizeMode::Fit;
        node->minHeight = barExtent;
    } else {
        node->widthMode = (opts.style == ToolbarStyle::Floating) ? SizeMode::Fit : SizeMode::Stretch;
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = barExtent;
    }

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;

    static constexpr PanelHooks kToolbarStaticHooks{
        .contentClip = [](const Node* n, const UiTheme&) -> Rect { return detail::Toolbar::GetContentClipRect(n); },
        .scrollToChild = &detail::Carousel::ScrollToChild,
        .collectBounds = &detail::Carousel::CollectBounds
    };
    static constexpr PanelHooks kToolbarScrollHooks{
        .contentClip = [](const Node* n, const UiTheme&) -> Rect { return detail::Toolbar::GetContentClipRect(n); },
        .update = &detail::Carousel::Update,
        .scrollToChild = &detail::Carousel::ScrollToChild,
        .collectBounds = &detail::Carousel::CollectBounds
    };
    node->panelHooks = &kToolbarStaticHooks;

    if (!isVertical && (opts.overflow == ToolbarOverflow::AutoCompact || opts.overflow == ToolbarOverflow::Scroll)) {
        auto* cData = ctx.AllocPayload<CarouselData>();
        cData->autoCompact = (opts.overflow == ToolbarOverflow::AutoCompact);
        node->SetPayload(cData);
        node->arrangeFn = &detail::Toolbar::Arrange;
        node->panelHooks = &kToolbarScrollHooks;
        node->postRenderFn = &detail::Carousel::RenderArrows;
        detail::Carousel::BeginScrollInteraction(ctx, node, cData, true, effectiveDisabled);
    }

    const auto& pal = theme.palette;
    const Node* surfaceParent = node->parent;
    while (surfaceParent != nullptr &&
           surfaceParent->backgroundColor.a == 0u &&
           (surfaceParent->type == NodeType::Row || surfaceParent->type == NodeType::Column) &&
           surfaceParent->padding.left == 0.0f && surfaceParent->padding.top == 0.0f) {
        surfaceParent = surfaceParent->parent;
    }
    const bool insideRoundedSurface =
        (opts.style == ToolbarStyle::Header) &&
        (surfaceParent != nullptr) &&
        (surfaceParent->cornerRadius > 0.0f) &&
        (surfaceParent->backgroundColor.a > 0u);

    const Color styleBg = (opts.style == ToolbarStyle::Floating)
        ? pal.surfaceElevated
        : (insideRoundedSurface ? Color::Transparent() : pal.surface);
    if (!opts.background.IsBackgroundEnabled(true)) {
        node->backgroundColor = Color::Transparent();
    } else if (opts.background.HasAny()) {
        node->backgroundColor = theme.ResolveColor(opts.background, false, false, effectiveDisabled, pal.surface);
    } else {
        node->backgroundColor = styleBg;
    }

    node->explicitRadius = (opts.border.radius.has_value() || opts.style != ToolbarStyle::Surface) ? 1 : 0;
    node->borderColor = theme.ResolveColor(opts.border.color, false, false, effectiveDisabled, pal.border);
    ctx.ApplySurfaceGradients(
        node,
        theme,
        opts.background.IsBackgroundEnabled(true) ? opts.background : StateColor{},
        opts.border.color,
        false,
        false,
        effectiveDisabled
    );
    node->SetSurfaceRole((opts.style == ToolbarStyle::Floating) ? SurfaceRole::Raised : SurfaceRole::DockEdge);
    const bool isFloatingPanel = (opts.style == ToolbarStyle::Floating) ||
        (opts.style == ToolbarStyle::Surface && theme.shellStyle == ShellStyle::Floating);
    node->SetDockSeam(isFloatingPanel
        ? DockSeam::None
        : (isVertical ? DockSeam::Right : DockSeam::Bottom));
    node->borderWidth = (opts.border.width > 0.0f) ? opts.border.width : theme.shell.panelBorderWidth;

    if (opts.border.radius.has_value()) {
        node->cornerRadius = *opts.border.radius;
    } else if (opts.style == ToolbarStyle::Header) {
        node->cornerRadius = 0.0f;
    } else if (opts.style == ToolbarStyle::Floating) {
        node->cornerRadius = theme.ResolveRadius(keel::kNullOpt, activeSize) + 2.0f;
    } else {
        node->cornerRadius = theme.shell.panelRadius;
    }

    const auto* tbCar = node->As<CarouselData>();
    node->uiSize = activeSize;
    node->visualHash = detail::HashArgs(
        keel::as<keel::u8>(theme.shellStyle),
        keel::as<keel::u8>(opts.style),
        keel::as<keel::u8>(opts.orientation),
        keel::as<keel::u8>(opts.overflow),
        keel::as<keel::u8>(opts.buttonStyle),
        node->cornerRadius,
        node->borderWidth,
        opts.background,
        opts.border,
        tbCar ? tbCar->leftArrowHovered : false,
        tbCar ? tbCar->leftArrowPressed : false,
        tbCar ? tbCar->rightArrowHovered : false,
        tbCar ? tbCar->rightArrowPressed : false
    );

    ctx.FinishContainer(node);

    return ToolbarScope(&ctx, node, res, opts);
}

Response detail::Toolbar::ToolButton(
    Context& ctx,
    const ToolButtonOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const Node* parent = ctx.GetCurrentContainer();
    const UiSize activeSize = opts.size.value_or((parent != nullptr) ? parent->uiSize : UiSize::Md);

    ctx.RegisterFocusable(id, effectiveDisabled);

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
    } else {
        if (res.clicked) {
            ctx.RequestFocus(id);
        }
        if (ctx.IsFocused(id) && (ctx.ConsumeKey(Key::Space) || ctx.ConsumeKey(Key::Enter))) {
            res.clicked = true;
        }
    }

    if (res.hovered) {
        const bool wasCompacted = opts.icon.IsValid() && !opts.text.empty() &&
                                  res.rect.w > 0.0f && res.rect.w <= res.rect.h + 2.0f;
        const UiText effTooltip = !opts.tooltip.empty()
            ? opts.tooltip
            : ((opts.square || wasCompacted) && !opts.text.empty() ? opts.text : UiText{});
        if (!effTooltip.empty()) {
            ctx.SetTooltip(effTooltip, res.rect);
        }
    }

    Node* node = ctx.CreateNode(NodeType::Button, id);

    auto* payload = ctx.AllocPayload<ToolButtonPayload>();
    payload->origText = ctx.AllocText(opts.text);
    payload->origSquare = opts.square;
    payload->btn.text = opts.square ? UiText{} : payload->origText;
    payload->btn.tooltip = ctx.AllocText(opts.tooltip);
    node->text = payload->btn.text.text;
    payload->btn.icon = ctx.AllocIcon(opts.icon);
    payload->btn.trailingIcon = ctx.AllocIcon(opts.trailingIcon);
    const ButtonStyle resolvedStyle = opts.style.value_or(ButtonStyle::Ghost);
    payload->btn.style = resolvedStyle;
    payload->btn.variant = opts.variant;
    payload->btn.background = opts.background;
    payload->btn.border = opts.border;
    payload->btn.iconPlacement = opts.iconPlacement;
    payload->btn.align = opts.align;
    payload->btn.width = opts.width;
    payload->btn.minWidth = opts.minWidth;
    payload->btn.disabled = effectiveDisabled;
    payload->btn.active = opts.active;
    payload->btn.square = opts.square;
    payload->btn.iconColor = opts.iconColor;
    payload->badge = ctx.AllocText(opts.badge);
    payload->badgeVariant = (opts.variant == UiVariant::Default) ? UiVariant::Primary : opts.variant;

    node->SetPayload(payload);
    node->visualHash = detail::HashArgs(
        keel::as<keel::u8>(resolvedStyle),
        keel::as<keel::u8>(opts.variant),
        opts.background,
        opts.border,
        opts.iconPlacement,
        opts.active,
        opts.square,
        opts.iconColor.value_or(Color::Transparent()),
        opts.width,
        opts.minWidth.value_or(0.0f),
        activeSize,
        keel::as<keel::u8>(payload->btn.icon.GetType()),
        keel::as<keel::u8>(payload->btn.icon.GetId()),
        payload->btn.icon.GetPath(),
        payload->origText,
        payload->badge
    );
    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        auto* p = const_cast<ToolButtonPayload*>(n->As<ToolButtonPayload>());
        if (p) {
            p->btn.square = p->origSquare;
            p->btn.text = p->origSquare ? UiText{} : p->origText;
        }
        Vec2 sz = detail::Button::Measure(p ? p->btn : ButtonOptions{}, n->uiSize, canvas, theme, availableWidth);
        if (p) {
            if (p->isSplit) {
                sz.x += p->splitChevronWidth;
            }
            p->expandedWidth = sz.x;
        }
        return sz;
    };
    node->renderFn = &RenderToolButtonNode;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->focused = ctx.IsFocusVisible(id);
    node->widthMode = opts.width.ResolveMode(SizeMode::Fit);
    node->fixedWidth = opts.width.IsFixed() ? opts.width.Dp() : 0.0f;
    if (opts.minWidth.has_value() && *opts.minWidth > 0.0f) {
        node->minWidth = *opts.minWidth;
    }
    node->heightMode = SizeMode::Fit;
    node->flexGrow = Control::ResolveFlexGrow(node->parent, opts.width, AxisSize::Fit(), 1.0f);

    ctx.AdaptChildToContainer(node);

    return res;
}

SplitButtonResponse detail::Toolbar::SplitButton(
    Context& ctx,
    const ToolButtonOptions& opts,
    keel::ArrayRef<ContextMenuItem> menuItems,
    ContextMenuOptions menuOpts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const Node* parent = ctx.GetCurrentContainer();
    const UiSize activeSize = opts.size.value_or((parent != nullptr) ? parent->uiSize : UiSize::Md);
    const keel::f32 chevW = 20.0f;

    const Rect prevUnclipped = ctx.GetPrevUnclippedBounds(id);
    const Rect prevClipped = ctx.GetPrevFrameBounds(id);
    const Rect prevRect = (prevUnclipped.w > 0.0f) ? prevUnclipped : prevClipped;
    const keel::f32 effChevW = (prevRect.w > chevW) ? chevW : (prevRect.w * 0.35f);
    const Rect chevRect(prevRect.Right() - effChevW, prevRect.y, effChevW, prevRect.h);

    const Vec2 mouse = ctx.GetMousePos();
    const Id activeId = ctx.GetActiveId();
    const bool otherActive = activeId.IsValid() && activeId != id && !ctx.IsMouseClicked();
    const bool overClipped = !otherActive && (prevClipped.w > 0.0f && prevClipped.h > 0.0f) && prevClipped.Contains(mouse);
    const bool overChevron = overClipped && chevRect.Contains(mouse) && !ctx.IsMouseOccluded(id, mouse);
    const bool inChevron = overChevron && activeId != id;

    ctx.RegisterFocusable(id, effectiveDisabled);

    Response baseRes{};
    if (!inChevron) {
        baseRes = ctx.Interact(id);
        if (overChevron) {
            baseRes.hovered = false;
            baseRes.clicked = false;
        }
        if (effectiveDisabled) {
            baseRes.clicked = false;
            baseRes.hovered = false;
            baseRes.pressed = false;
        } else {
            if (baseRes.clicked) {
                ctx.RequestFocus(id);
            }
            if (ctx.IsFocused(id) && (ctx.ConsumeKey(Key::Space) || ctx.ConsumeKey(Key::Enter))) {
                baseRes.clicked = true;
            }
        }
    } else {
        baseRes.id = id;
        baseRes.ctx = &ctx;
        baseRes.rect = prevRect;
    }

    Node* node = ctx.CreateNode(NodeType::Button, id);

    auto* payload = ctx.AllocPayload<ToolButtonPayload>();
    payload->origText = ctx.AllocText(opts.text);
    payload->origSquare = opts.square;
    payload->btn.text = opts.square ? UiText{} : payload->origText;
    payload->btn.tooltip = ctx.AllocText(opts.tooltip);
    node->text = payload->btn.text.text;
    payload->btn.icon = ctx.AllocIcon(opts.icon);
    payload->btn.trailingIcon = ctx.AllocIcon(opts.trailingIcon);
    const ButtonStyle resolvedStyle = opts.style.value_or(ButtonStyle::Ghost);
    payload->btn.style = resolvedStyle;
    payload->btn.variant = opts.variant;
    payload->btn.background = opts.background;
    payload->btn.border = opts.border;
    payload->btn.iconPlacement = opts.iconPlacement;
    payload->btn.align = opts.align;
    payload->btn.width = opts.width;
    payload->btn.minWidth = opts.minWidth;
    payload->btn.disabled = effectiveDisabled;
    payload->btn.active = opts.active;
    payload->btn.square = opts.square;
    payload->btn.iconColor = opts.iconColor;
    payload->badge = ctx.AllocText(opts.badge);
    payload->badgeVariant = (opts.variant == UiVariant::Default) ? UiVariant::Primary : opts.variant;
    payload->isSplit = true;
    payload->splitChevronWidth = chevW;
    payload->chevronHovered = !effectiveDisabled && inChevron;

    struct SplitDropdownState {
        bool open{false};
    };
    auto& dropState = ctx.GetOrCreateState<SplitDropdownState>(id);
    if (!effectiveDisabled && inChevron && ctx.IsMouseClicked()) {
        dropState.open = !dropState.open;
        ctx.ConsumeMouseClick();
        ctx.InvalidateAll();
    }

    menuOpts.disabled = effectiveDisabled;
    menuOpts.size = activeSize;
    menuOpts.anchorRect = chevRect;
    if (!menuOpts.minWidth.has_value()) {
        menuOpts.minWidth = keel::max(160.0f, prevRect.w);
    }
    if (!menuOpts.anchorPos.has_value() && prevRect.w > 0.0f && prevRect.h > 0.0f) {
        menuOpts.anchorPos = Vec2{prevRect.x, prevRect.Bottom() + 4.0f};
    }
    const ContextMenuResponse menuRes = detail::ContextMenu::Run(ctx, dropState.open, menuItems, menuOpts, loc);
    payload->chevronOpen = menuRes.isOpen;
    payload->chevronPressed = !effectiveDisabled && inChevron && (ctx.IsMouseClicked() || menuRes.isOpen);

    if ((baseRes.hovered || payload->chevronHovered) && !menuRes.isOpen) {
        const bool wasCompacted = opts.icon.IsValid() && !opts.text.empty() &&
                                  prevRect.w > 0.0f && prevRect.w <= prevRect.h + chevW + 2.0f;
        const UiText effTooltip = !opts.tooltip.empty()
            ? opts.tooltip
            : ((opts.square || wasCompacted) && !opts.text.empty() ? opts.text : UiText{});
        if (!effTooltip.empty()) {
            ctx.SetTooltip(effTooltip, prevRect);
        }
    }

    node->SetPayload(payload);
    node->visualHash = detail::HashArgs(
        keel::as<keel::u8>(resolvedStyle),
        keel::as<keel::u8>(opts.variant),
        opts.background,
        opts.border,
        opts.iconPlacement,
        opts.active,
        opts.square,
        opts.iconColor.value_or(Color::Transparent()),
        opts.width,
        opts.minWidth.value_or(0.0f),
        activeSize,
        keel::as<keel::u8>(payload->btn.icon.GetType()),
        keel::as<keel::u8>(payload->btn.icon.GetId()),
        payload->btn.icon.GetPath(),
        payload->origText,
        payload->badge,
        payload->isSplit,
        payload->chevronHovered,
        payload->chevronPressed,
        payload->chevronOpen
    );
    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        auto* p = const_cast<ToolButtonPayload*>(n->As<ToolButtonPayload>());
        if (p) {
            p->btn.square = p->origSquare;
            p->btn.text = p->origSquare ? UiText{} : p->origText;
        }
        Vec2 sz = detail::Button::Measure(p ? p->btn : ButtonOptions{}, n->uiSize, canvas, theme, availableWidth);
        if (p) {
            sz.x += p->splitChevronWidth;
            p->expandedWidth = sz.x;
        }
        return sz;
    };
    node->renderFn = &RenderToolButtonNode;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = baseRes.hovered;
    node->pressed = baseRes.pressed;
    node->focused = ctx.IsFocusVisible(id);
    if (opts.width.IsFixed()) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = opts.width.Dp() + chevW;
    } else {
        node->widthMode = opts.width.ResolveMode(SizeMode::Fit);
        node->fixedWidth = 0.0f;
    }
    if (opts.minWidth.has_value() && *opts.minWidth > 0.0f) {
        node->minWidth = *opts.minWidth + chevW;
    }
    node->heightMode = SizeMode::Fit;
    node->flexGrow = Control::ResolveFlexGrow(node->parent, opts.width, AxisSize::Fit(), 1.0f);

    ctx.AdaptChildToContainer(node);

    SplitButtonResponse res{};
    static_cast<Response&>(res) = baseRes;
    res.toggled = baseRes.clicked;
    res.selectedIndex = menuRes.clickedIndex;
    res.selectionChanged = (menuRes.clickedIndex >= 0);
    res.changed = res.toggled || res.selectionChanged;
    res.isOpen = menuRes.isOpen;
    return res;
}

void detail::Toolbar::RenderSeparator(
    ICanvas& canvas,
    Rect bounds,
    Color color,
    const UiTheme& theme,
    Orientation orientation
) {
    detail::Separator::Render(
        canvas,
        bounds,
        color,
        theme,
        orientation,
        orientation == Orientation::Vertical ? 0.18f : 0.20f
    );
}

void detail::Toolbar::Arrange(
    Node* node,
    const Rect& availableRect,
    const UiTheme& theme
) {
    if (!node) return;

    keel::f32 w = availableRect.w;
    keel::f32 h = (node->heightMode == SizeMode::Fixed) ? node->fixedHeight : availableRect.h;
    if (node->parent == nullptr && !node->isSpacer && node->widthMode == SizeMode::Fit) {
        w = node->intrinsicSize.x;
    }

    node->computedRect = Rect(availableRect.x, availableRect.y, w, h);
    if (node->childCount == 0) return;

    auto arrangeFittingRow = [&](keel::f32 contentTotalW) {
        keel::f32 boxW = w;
        keel::f32 boxX = availableRect.x;
        if (node->widthMode == SizeMode::Fit) {
            boxW = keel::min(w, contentTotalW + node->padding.Horizontal());
            if (node->parent != nullptr) {
                const bool centerInCol = (node->parent->direction == FlexDirection::Column && node->parent->align == Alignment::Center);
                const bool centerInRow = (node->parent->direction == FlexDirection::Row && node->parent->justify == Justify::Center);
                const bool endInCol = (node->parent->direction == FlexDirection::Column && node->parent->align == Alignment::End);
                const bool endInRow = (node->parent->direction == FlexDirection::Row && node->parent->justify == Justify::End);
                if (centerInCol || centerInRow) {
                    boxX = availableRect.x + (availableRect.w - boxW) * 0.5f;
                } else if (endInCol || endInRow) {
                    boxX = availableRect.Right() - boxW;
                }
            }
        }
        node->computedRect.x = boxX;
        node->computedRect.w = boxW;
        const keel::f32 sX = boxX + node->padding.left;
        const keel::f32 sY = node->computedRect.y + node->padding.top;
        const keel::f32 fW = keel::max(0.0f, boxW - node->padding.Horizontal());
        const keel::f32 fH = keel::max(0.0f, node->computedRect.h - node->padding.Vertical());

        if (auto* cData = node->As<CarouselData>()) {
            cData->showLeftArrow = false;
            cData->showRightArrow = false;
            cData->leftArrowRect = Rect(0.0f, 0.0f, 0.0f, 0.0f);
            cData->rightArrowRect = Rect(0.0f, 0.0f, 0.0f, 0.0f);
            cData->viewportRect = Rect(sX, sY, fW, fH);
            cData->targetOffsetX = 0.0f;
            cData->animatingX = false;
            cData->stepDirection = 0;
        }
        if (node->scroll) {
            node->scroll->offset.x = 0.0f;
        }

        keel::f32 nonFlexSum = 0.0f;
        keel::f32 totalFlexGrow = 0.0f;
        keel::u32 flowCount = 0;
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->IsOutOfFlow()) continue;
            ++flowCount;
            if (child->isSpacer || child->widthMode == SizeMode::Stretch) {
                totalFlexGrow += keel::max(0.01f, child->flexGrow);
            } else if (child->widthMode == SizeMode::Fixed) {
                nonFlexSum += child->fixedWidth;
            } else {
                nonFlexSum += child->intrinsicSize.x;
            }
        }

        const keel::f32 totalGap = (flowCount > 1) ? keel::as<keel::f32>(flowCount - 1) * node->gap : 0.0f;
        const keel::f32 freeSpace = keel::max(0.0f, fW - totalGap - nonFlexSum);
        if (node->scroll) {
            node->scroll->contentSize = {nonFlexSum + totalGap, fH};
        }

        keel::f32 curX = sX;
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->IsOutOfFlow()) continue;
            keel::f32 childW = 0.0f;
            if (child->isSpacer || child->widthMode == SizeMode::Stretch) {
                childW = (totalFlexGrow > 0.0f)
                    ? (freeSpace * (keel::max(0.01f, child->flexGrow) / totalFlexGrow))
                    : 0.0f;
            } else if (child->widthMode == SizeMode::Fixed) {
                childW = child->fixedWidth;
            } else {
                childW = child->intrinsicSize.x;
            }
            const keel::f32 childH = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight :
                                 (child->heightMode == SizeMode::Stretch ? fH : child->intrinsicSize.y);
            keel::f32 childY = sY;
            if (node->align == Alignment::Center) {
                childY += (fH - childH) * 0.5f;
            } else if (node->align == Alignment::End) {
                childY += (fH - childH);
            }
            child->computedRect = Rect(curX, childY, childW, childH);
            Solver::Arrange(child, child->computedRect, theme);
            curX += childW + node->gap;
        }
    };

    const keel::f32 availContentW = keel::max(0.0f, w - node->padding.Horizontal());
    keel::f32 expandedTotalW = 0.0f;
    keel::u32 flowCount = 0;

    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        if (child->IsOutOfFlow()) continue;
        if (child->type == NodeType::Button) {
            if (auto* p = child->As<ToolButtonPayload>()) {
                p->btn.square = p->origSquare;
                p->btn.text = p->origSquare ? UiText{} : p->origText;
                child->text = p->btn.text.text;
                if (p->expandedWidth > 0.0f) {
                    child->intrinsicSize.x = p->expandedWidth;
                }
            }
        }
        ++flowCount;
        if (!child->isSpacer) {
            const keel::f32 cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
            expandedTotalW += cw;
        }
    }
    if (flowCount > 1) {
        expandedTotalW += keel::as<keel::f32>(flowCount - 1) * node->gap;
    }

    if (expandedTotalW <= availContentW + 0.5f) {
        arrangeFittingRow(expandedTotalW);
        return;
    }

    auto* cData = node->As<CarouselData>();
    if (cData && cData->autoCompact) {
        keel::f32 compactTotalW = 0.0f;
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->IsOutOfFlow()) continue;
            if (child->type == NodeType::Button) {
                if (auto* p = child->As<ToolButtonPayload>()) {
                    if (!p->origSquare && p->btn.icon.IsValid() && !p->origText.empty() &&
                        p->btn.iconPlacement == Placement::Leading) {
                        p->btn.square = true;
                        p->btn.text = UiText{};
                        child->text = {};
                        child->intrinsicSize.x = detail::Button::Measure(p->btn, child->uiSize, nullptr, theme).x +
                                                 (p->isSplit ? p->splitChevronWidth : 0.0f);
                    }
                }
            }
            if (!child->isSpacer) {
                const keel::f32 cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                compactTotalW += cw;
            }
        }
        if (flowCount > 1) {
            compactTotalW += keel::as<keel::f32>(flowCount - 1) * node->gap;
        }

        if (compactTotalW <= availContentW + 0.5f) {
            arrangeFittingRow(compactTotalW);
            return;
        }
    }

    detail::Carousel::Arrange(node, availableRect, theme);
}

Rect detail::Toolbar::GetContentClipRect(const Node* node) noexcept {
    if (!node) return Rect{};
    if (const auto* cData = node->As<CarouselData>()) {
        if (cData->viewportRect.w > 0.0f || cData->viewportRect.h > 0.0f) {
            return Rect(
                cData->viewportRect.x,
                node->computedRect.y,
                cData->viewportRect.w,
                node->computedRect.h
            );
        }
    }
    return Rect(
        node->computedRect.x + node->padding.left,
        node->computedRect.y,
        keel::max(0.0f, node->computedRect.w - node->padding.Horizontal()),
        node->computedRect.h
    );
}

} // namespace guikit
