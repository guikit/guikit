#include "guikit/actions/split_button.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/node.hpp"
#include "guikit/style/style_painter.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

struct SplitButtonState {
    bool open{false};
    Id popupId{};
};

struct SplitButtonPayload {
    ButtonOptions btn{};
    keel::f32 loadingPhase{0.0f};
    keel::f32 chevronWidth{24.0f};
    bool chevronHovered{false};
    bool chevronPressed{false};
    bool chevronOpen{false};
};

struct ResolvedSplitPalette {
    Color baseBg{Color::Transparent()};
    Color mainBg{Color::Transparent()};
    Color chevBg{Color::Transparent()};
    Color textCol{Color::Transparent()};
    Color borderCol{Color::Transparent()};
    keel::f32 borderW{0.0f};
    bool isGhost{false};
};

keel::f32 ComputeChevronWidth(const ComponentMetrics& m) noexcept {
    return keel::round(keel::clamp(m.defaultHeight * 0.72f, 20.0f, 32.0f));
}

ControlSpec EffectiveSplitControl(const ButtonOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const ComponentMetrics& m = theme.GetMetrics(size);
    const auto& inputMetrics = theme.GetInputField(size);
    return Control::FromOptions(
        opts,
        SizeMode::Fit,
        SizeMode::Fit,
        ElementLayout::Stacked,
        inputMetrics.labelGap,
        inputMetrics.descGap,
        m.paddingX
    );
}

ResolvedSplitPalette ResolveSplitColors(
    const ButtonOptions& opts,
    const UiTheme& theme,
    bool mainHov,
    bool mainPress,
    bool chevHov,
    bool chevPress,
    bool disabled,
    bool focused
) noexcept {
    ResolvedSplitPalette out{};
    const auto& pal = theme.palette;
    const bool isDark = (theme.mode == ThemeMode::Dark);
    const bool hasCtrlBg = opts.background.IsBackgroundEnabled(true);
    const bool anyHov = mainHov || chevHov;
    const bool anyPress = mainPress || chevPress;
    const bool isGhostBtn = (opts.style == ButtonStyle::Ghost) ||
                            (opts.style == ButtonStyle::Default && opts.variant == UiVariant::Ghost);
    const bool isSemantic = (opts.variant == UiVariant::Primary ||
                             opts.variant == UiVariant::Success ||
                             opts.variant == UiVariant::Warning ||
                             opts.variant == UiVariant::Danger  ||
                             opts.variant == UiVariant::Info);

    out.isGhost = isGhostBtn;
    out.borderW = (opts.border.width > 0.0f) ? opts.border.width : 0.0f;

    Color restBg = Color::Transparent();
    Color hovBg = Color::Transparent();
    Color actBg = Color::Transparent();
    Color defaultText = pal.textPrimary;

    if (isGhostBtn) {
        const VariantColors& ghost = theme.GetVariant(UiVariant::Ghost);
        if (isSemantic) {
            const Color seed = theme.GetSemanticAccent(opts.variant);
            defaultText = theme.EnsureReadableInk(seed, pal.surface);
            if (hasCtrlBg) {
                hovBg = seed.WithAlphaF(theme.SoftTintAlpha(false, false));
                actBg = seed.WithAlphaF(theme.SoftTintAlpha(false, true));
            }
        } else {
            defaultText = ghost.text;
            if (hasCtrlBg) {
                hovBg = ghost.bgHover;
                actBg = ghost.bgActive;
            }
        }
        out.borderCol = (opts.border.width > 0.0f) ? pal.border : ghost.border;
        if (!disabled && (opts.active || anyHov)) {
            out.borderCol = opts.active ? defaultText.WithAlphaF(0.35f) : pal.borderSubtle;
            out.borderW = keel::max(out.borderW, 1.0f);
        }
    } else if (opts.style == ButtonStyle::Default) {
        const UiVariant effVar = isSemantic ? opts.variant : UiVariant::Secondary;
        const VariantColors& vc = theme.GetVariant(effVar);
        defaultText = vc.text;
        out.borderCol = vc.border;
        if (opts.border.width <= 0.0f) out.borderW = vc.borderWidth;
        if (hasCtrlBg) {
            restBg = vc.bg;
            hovBg = vc.bgHover;
            actBg = vc.bgActive;
        }
    } else if (opts.style == ButtonStyle::Solid) {
        const Color seed = isSemantic ? theme.GetSemanticAccent(opts.variant) : pal.accent;
        defaultText = Color::BestTextColor(seed, Color::Hex(0x0F172AFF), Color::Hex(0xFFFFFFFF));
        if (hasCtrlBg) {
            restBg = seed;
            hovBg = isDark ? seed.Lighten(0.08f) : seed.Lighten(0.07f);
            actBg = isDark ? seed.Darken(0.08f) : seed.Darken(0.09f);
        }
        if (opts.border.width > 0.0f) {
            out.borderCol = defaultText.WithAlphaF(0.65f);
        }
    } else if (opts.style == ButtonStyle::Soft) {
        const Color seed = isSemantic ? theme.GetSemanticAccent(opts.variant) : pal.accent;
        const Color softRest = Color::Lerp(pal.surface, seed, isDark ? 0.22f : 0.15f).WithAlpha(255u);
        defaultText = theme.EnsureReadableInk(isDark ? seed.Lighten(0.18f) : seed.Darken(0.16f), softRest);
        out.borderCol = Color::Lerp(pal.border, seed, isDark ? 0.45f : 0.40f).WithAlpha(255u);
        if (opts.border.width <= 0.0f) out.borderW = 1.0f * keel::max(theme.borderScale, 0.5f);
        if (hasCtrlBg) {
            restBg = softRest;
            hovBg = Color::Lerp(pal.surface, seed, isDark ? 0.32f : 0.23f).WithAlpha(255u);
            actBg = Color::Lerp(pal.surface, seed, isDark ? 0.42f : 0.31f).WithAlpha(255u);
        }
    } else {
        // ButtonStyle::Outline
        const Color seed = isSemantic ? theme.GetSemanticAccent(opts.variant) : pal.borderHover;
        defaultText = isSemantic ? theme.EnsureReadableInk(seed, pal.surface) : pal.textPrimary;
        out.borderCol = isSemantic ? seed.WithAlphaF(0.85f) : pal.borderHover;
        if (opts.border.width <= 0.0f) out.borderW = 1.25f * keel::max(theme.borderScale, 0.5f);
        if (hasCtrlBg) {
            hovBg = isSemantic ? seed.WithAlphaF(theme.SoftTintAlpha(false, false)) : pal.surfaceHover;
            actBg = isSemantic ? seed.WithAlphaF(theme.SoftTintAlpha(false, true)) : pal.surfaceActive;
        }
    }

    if (hasCtrlBg && opts.background.HasAny() && !opts.background.IsNone()) {
        if (opts.background.IsSurface()) {
            if (opts.background.HasAlphaScale()) {
                restBg = restBg.ScaleAlpha(opts.background.alphaScale);
                hovBg = hovBg.ScaleAlpha(opts.background.alphaScale);
                actBg = actBg.ScaleAlpha(opts.background.alphaScale);
            }
        } else {
            restBg = theme.ResolveColor(opts.background, false, opts.active, disabled, restBg);
            hovBg = theme.ResolveColor(opts.background, true, false, disabled, hovBg);
            actBg = theme.ResolveColor(opts.background, false, true, disabled, actBg);
            if (!opts.text.color.HasAny() && restBg.a >= 180u) {
                defaultText = Color::BestTextColor(restBg, Color::Hex(0x0F172AFF), Color::Hex(0xFFFFFFFF));
            }
        }
    }

    if (disabled) {
        out.baseBg = restBg.WithAlphaF(theme.disabledOpacity);
        out.mainBg = out.baseBg;
        out.chevBg = out.baseBg;
        out.borderCol = out.borderCol.WithAlphaF(theme.disabledOpacity);
    } else {
        out.baseBg = opts.active ? actBg : restBg;
        out.mainBg = mainPress ? actBg : (mainHov ? hovBg : out.baseBg);
        out.chevBg = chevPress ? actBg : (chevHov ? hovBg : out.baseBg);
    }

    out.textCol = opts.text.color.HasAny()
        ? theme.ResolveTextStateColor(opts.text.color).Resolve(anyHov, anyPress || opts.active, disabled, defaultText)
        : (disabled ? defaultText.WithAlphaF(theme.disabledOpacity) : defaultText);

    if (opts.border.color.HasAny()) {
        out.borderCol = theme.ResolveColor(opts.border.color, anyHov, anyPress || opts.active || focused, disabled, out.borderCol);
    }

    if (focused && !disabled) {
        if (!opts.border.color.active.has_value()) {
            out.borderCol = pal.focusRing;
        }
        out.borderW = keel::max(out.borderW, theme.focusRingWidth);
    }

    return out;
}

void RenderSplitButtonNode(const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
    const auto* p = n->As<SplitButtonPayload>();
    if (!p) return;

    const Rect fullRect = n->computedRect;
    if (fullRect.w <= 0.0f || fullRect.h <= 0.0f) return;

    const ComponentMetrics& m = theme.GetMetrics(n->uiSize);
    const keel::f32 chevW = keel::min(p->chevronWidth, fullRect.w * 0.45f);
    const Rect mainRect(fullRect.x, fullRect.y, keel::max(0.0f, fullRect.w - chevW), fullRect.h);
    const Rect chevRect(fullRect.Right() - chevW, fullRect.y, chevW, fullRect.h);

    const bool mainHov = n->hovered;
    const bool mainPress = n->pressed;
    const bool chevHov = p->chevronHovered || p->chevronOpen;
    const bool chevPress = p->chevronPressed || p->chevronOpen;
    const bool anyHov = mainHov || chevHov;
    const bool anyPress = mainPress || chevPress;

    const ResolvedSplitPalette palRes = ResolveSplitColors(
        p->btn, theme, mainHov, mainPress, chevHov, chevPress, n->disabled, n->focused
    );
    const keel::f32 radius = p->btn.border.radius.value_or(m.cornerRadius);
    const FrameKind frameKind = palRes.isGhost ? FrameKind::ToolbarButton : FrameKind::Button;

    theme.Painter().DrawFrame(canvas, theme, frameKind, FramePaintParams{
        .rect = fullRect,
        .radius = radius,
        .explicitRadius = p->btn.border.radius.has_value(),
        .bg = palRes.baseBg,
        .border = palRes.borderCol,
        .borderWidth = palRes.borderW,
        .hovered = anyHov,
        .pressed = anyPress,
        .active = p->btn.active,
        .focused = n->focused && !n->disabled,
        .disabled = n->disabled,
        .accent = theme.palette.accent
    });

    canvas.PushClip(fullRect);

    if (palRes.mainBg.a > 0 && palRes.mainBg != palRes.baseBg && mainRect.w > 0.0f) {
        canvas.PushClip(mainRect);
        canvas.DrawRoundRect(fullRect, radius, palRes.mainBg, Color::Transparent(), 0.0f);
        canvas.PopClip();
    }
    if (palRes.chevBg.a > 0 && palRes.chevBg != palRes.baseBg && chevRect.w > 0.0f) {
        canvas.PushClip(chevRect);
        canvas.DrawRoundRect(fullRect, radius, palRes.chevBg, Color::Transparent(), 0.0f);
        canvas.PopClip();
    }

    // Left primary action content (delegated to Button::Render with transparent chrome)
    const keel::f32 mainShift = theme.Painter().GetButtonContentOffset(theme, mainPress, p->btn.active, n->disabled);
    const Rect shiftedMainRect(mainRect.x + mainShift, mainRect.y + mainShift, mainRect.w, mainRect.h);
    ButtonOptions mainBtn = p->btn;
    mainBtn.style = ButtonStyle::Ghost;
    mainBtn.background = StateColor::None();
    mainBtn.border = {.width = 0.0f};
    mainBtn.active = false;
    mainBtn.trailingIcon = {};
    mainBtn.text.color = StateColor{palRes.textCol, palRes.textCol, palRes.textCol, palRes.textCol};
    if (!mainBtn.iconColor.has_value()) {
        mainBtn.iconColor = palRes.textCol;
    }
    detail::Button::Render(
        canvas,
        shiftedMainRect,
        mainBtn,
        n->uiSize,
        false,
        false,
        n->disabled,
        theme,
        false,
        p->loadingPhase
    );

    // Subtle vertical divider separating primary action from chevron trigger
    const keel::f32 sepX = keel::floor(chevRect.x) + 0.5f;
    const keel::f32 sepPadY = keel::round(fullRect.h * 0.18f);
    const Color sepBase = (p->btn.style == ButtonStyle::Solid || p->btn.variant == UiVariant::Primary ||
                           p->btn.variant == UiVariant::Success || p->btn.variant == UiVariant::Warning ||
                           p->btn.variant == UiVariant::Danger  || p->btn.variant == UiVariant::Info)
        ? palRes.textCol.WithAlphaF(n->disabled ? 0.14f : 0.28f)
        : theme.palette.border.WithAlphaF(n->disabled ? 0.20f : (anyHov ? 0.85f : 0.55f));
    theme.Painter().DrawDivider(
        canvas,
        theme,
        Vec2(sepX, fullRect.y + sepPadY),
        Vec2(sepX, fullRect.Bottom() - sepPadY),
        sepBase,
        1.0f
    );

    // Right chevron glyph
    const keel::f32 chevShift = theme.Painter().GetButtonContentOffset(theme, chevPress, false, n->disabled);
    const keel::f32 chevIconSz = keel::round(m.iconSize * 0.78f);
    const Rect chevIconRect(
        chevRect.x + (chevRect.w - chevIconSz) * 0.5f + chevShift,
        chevRect.y + (chevRect.h - chevIconSz) * 0.5f + chevShift,
        chevIconSz,
        chevIconSz
    );
    const Color chevCol = palRes.textCol.WithAlphaF(n->disabled ? theme.disabledOpacity : (chevHov ? 0.96f : 0.80f));
    Icon::ChevronDown().Draw(canvas, chevIconRect, chevCol);

    canvas.PopClip();
}

struct SplitPrepared {
    Id menuId{};
    Response baseRes{};
    SplitButtonPayload* payload{nullptr};
    SplitButtonState* state{nullptr};
};

SplitPrepared RunSplitButtonShell(
    Context& ctx,
    ButtonOptions opts,
    ContextMenuOptions& menuOpts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const bool interactionBlocked = effectiveDisabled || opts.loading;
    const UiSize activeSize = opts.size;
    const ComponentMetrics& metrics = ctx.GetTheme().GetMetrics(activeSize);
    const keel::f32 chevW = ComputeChevronWidth(metrics);

    auto idScope = ctx.IdScope(id.value);
    const Id menuId = ctx.MakeChildId(keel::SourceLocation::current());

    auto& dropState = ctx.GetOrCreateState<SplitButtonState>(id);

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

    ctx.RegisterFocusable(id, interactionBlocked);

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
        } else if (opts.loading) {
            baseRes.clicked = false;
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

    if (interactionBlocked) {
        dropState.open = false;
    } else if (inChevron && ctx.IsMouseClicked()) {
        dropState.open = !dropState.open;
        ctx.ConsumeMouseClick();
        ctx.InvalidateAll();
    } else if (ctx.IsFocused(id) && ctx.ConsumeKey(Key::Down)) {
        if (!dropState.open) {
            dropState.open = true;
            menuOpts.autoFocusFirst = true;
            ctx.InvalidateAll();
        }
    }

    keel::f32 loadingPhase = 0.0f;
    if (opts.loading && !effectiveDisabled) {
        loadingPhase = keel::as<keel::f32>(keel::fmod(keel::max(0.0, ctx.GetCurrentTime()) * 1.35, 1.0));
        ctx.RequestRepaint(2);
    }

    ButtonOptions optsCopy = opts;
    optsCopy.square = false;
    optsCopy.trailingIcon = {};
    optsCopy.text = ctx.AllocText(opts.text);
    optsCopy.badge = ctx.AllocText(opts.badge);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);
    if (!opts.icon.GetPath().empty()) {
        optsCopy.icon = Icon::File(ctx.AllocString(opts.icon.GetPath()));
    }

    const bool visualOpen = dropState.open && dropState.popupId.IsValid() && ctx.IsPopupOpen(dropState.popupId);
    const bool chevHovered = !interactionBlocked && inChevron;
    const bool chevPressed = !interactionBlocked && inChevron && (ctx.IsMouseDown() || visualOpen);

    if ((baseRes.hovered || chevHovered) && !visualOpen && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, prevRect);
    }

    Node* node = ctx.CreateNode(NodeType::Button, id);
    auto* payload = ctx.AllocPayload<SplitButtonPayload>(SplitButtonPayload{
        .btn = optsCopy,
        .loadingPhase = loadingPhase,
        .chevronWidth = chevW,
        .chevronHovered = chevHovered,
        .chevronPressed = chevPressed,
        .chevronOpen = visualOpen
    });
    node->text = payload->btn.text.text;
    node->SetPayload(payload);

    const ControlSpec ctrl = EffectiveSplitControl(payload->btn, activeSize, ctx.GetTheme());
    node->visualHash = detail::HashArgs(
        keel::as<keel::u8>(payload->btn.style),
        keel::as<keel::u8>(payload->btn.variant),
        payload->btn.iconPlacement,
        keel::as<keel::u8>(payload->btn.align),
        payload->btn.active,
        payload->btn.loading,
        loadingPhase,
        payload->btn.iconColor.value_or(Color::Transparent()),
        payload->btn.width,
        payload->btn.height,
        activeSize,
        payload->btn.background,
        payload->btn.border,
        keel::as<keel::u8>(payload->btn.icon.GetType()),
        keel::as<keel::u8>(payload->btn.icon.GetId()),
        payload->btn.icon.GetPath(),
        payload->btn.text,
        payload->btn.badge,
        chevW,
        chevHovered,
        chevPressed,
        visualOpen
    );

    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* p = n->As<SplitButtonPayload>();
        if (!p) return {};
        ButtonOptions inner = p->btn;
        inner.width = AxisSize::Fit();
        inner.minWidth = keel::kNullOpt;
        inner.maxWidth = keel::kNullOpt;
        Vec2 sz = detail::Button::Measure(inner, n->uiSize, canvas, theme, availableWidth);
        sz.x += p->chevronWidth;
        const ControlSpec c = EffectiveSplitControl(p->btn, n->uiSize, theme);
        if (c.width.IsFixed()) {
            sz.x = c.width.Dp();
        } else if (c.minWidth.has_value()) {
            sz.x = keel::max(*c.minWidth, sz.x);
        }
        return Control::Measure(sz, c, theme);
    };
    node->renderFn = &RenderSplitButtonNode;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = baseRes.hovered;
    node->pressed = baseRes.pressed;
    node->focused = ctx.IsFocusVisible(id);

    Control::SetupNode(ctx, node, ctrl);

    menuOpts.disabled = interactionBlocked;
    if (menuOpts.size == UiSize::Md && activeSize != UiSize::Md) {
        menuOpts.size = activeSize;
    }
    menuOpts.anchorRect = chevRect;
    if (!menuOpts.minWidth.has_value()) {
        menuOpts.minWidth = keel::max(160.0f, prevRect.w);
    }
    if (!menuOpts.anchorPos.has_value() && prevRect.w > 0.0f && prevRect.h > 0.0f) {
        menuOpts.anchorPos = Vec2{prevRect.x, prevRect.Bottom() + 4.0f};
    }

    return SplitPrepared{
        .menuId = menuId,
        .baseRes = baseRes,
        .payload = payload,
        .state = &dropState
    };
}

} // namespace

SplitButtonResponse detail::SplitButton::Run(
    Context& ctx,
    keel::ArrayRef<ContextMenuItem> items,
    ButtonOptions opts,
    ContextMenuOptions menuOpts,
    keel::SourceLocation loc
) {
    const SplitPrepared prep = RunSplitButtonShell(ctx, keel::move(opts), menuOpts, loc);
    auto scope = ctx.IdScope(prep.menuId.value);
    const ContextMenuResponse menuRes = detail::ContextMenu::Run(
        ctx,
        prep.state->open,
        items,
        menuOpts,
        keel::SourceLocation::current()
    );
    prep.state->popupId = menuRes.id;
    if (prep.payload) {
        prep.payload->chevronOpen = menuRes.isOpen;
    }

    SplitButtonResponse out{};
    static_cast<Response&>(out) = prep.baseRes;
    out.toggled = prep.baseRes.clicked;
    out.selectionChanged = menuRes.changed;
    out.selectedIndex = menuRes.clickedIndex;
    out.clickedId = menuRes.clickedId;
    out.isOpen = menuRes.isOpen;
    out.changed = out.toggled || out.selectionChanged;
    return out;
}

MenuScope detail::SplitButton::Open(
    Context& ctx,
    bool& primaryClicked,
    ButtonOptions opts,
    ContextMenuOptions menuOpts,
    keel::SourceLocation loc
) {
    const SplitPrepared prep = RunSplitButtonShell(ctx, keel::move(opts), menuOpts, loc);
    primaryClicked = prep.baseRes.clicked;
    prep.state->popupId = prep.menuId;
    return detail::ContextMenu::OpenWithId(ctx, prep.menuId, prep.state->open, menuOpts);
}

} // namespace guikit
