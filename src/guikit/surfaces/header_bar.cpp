#include "guikit/surfaces/header_bar.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/display/separator.hpp"
#include "guikit/layout/spacer.hpp"
#include "guikit/style/style_painter.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

struct HeaderBarRenderPayload {
    UiText centeredTitle{};
    UiText centeredSubtitle{};
    Icon centeredIcon{};
    bool centerTitle{false};
    bool focused{true};
    bool explicitBg{false};
    Color bgColor{};
    bool drawBottomBorder{true};
    Color borderColor{};
    keel::f32 borderWidth{1.0f};
    keel::f32 topRadius{0.0f};
    UiSize size{UiSize::Md};
};

struct HeaderTitleBlockPayload {
    UiText title{};
    UiText subtitle{};
    Icon icon{};
    bool focused{true};
    UiSize size{UiSize::Md};
};

struct WinCtrlButtonPayload {
    WindowControlKind kind{WindowControlKind::Minimize};
    bool maximized{false};
    bool focused{true};
    bool macStyle{false};
    bool hovered{false};
    bool pressed{false};
    bool disabled{false};
    keel::f32 width{38.0f};
    keel::f32 height{28.0f};
};

[[nodiscard]] keel::f32 ResolveHeaderBarHeight(const UiTheme& theme, UiSize size, bool focused) noexcept {
    const HeaderBarChromeMetrics cm = theme.Painter().GetHeaderBarMetrics(theme, focused);
    keel::f32 baseH = 38.0f;
    switch (size.GetPreset()) {
        case UiSize::Preset::Xs: baseH = 30.0f; break;
        case UiSize::Preset::Sm: baseH = 34.0f; break;
        case UiSize::Preset::Lg: baseH = 44.0f; break;
        case UiSize::Preset::Xl: baseH = 48.0f; break;
        case UiSize::Preset::Md:
        case UiSize::Preset::None:
            baseH = 38.0f;
            break;
        default:
            baseH = keel::round(38.0f * theme.SizeFactor(size));
            break;
    }
    return keel::max(24.0f, baseH + cm.defaultHeightDelta);
}

[[nodiscard]] WindowControlsPlacement ResolveControlsPlacement(
    const HeaderBarOptions& opts
) noexcept {
    if (!opts.showWindowControls || opts.controlsPlacement == WindowControlsPlacement::None) {
        return WindowControlsPlacement::None;
    }
    if (opts.controlsPlacement != WindowControlsPlacement::Auto) {
        return opts.controlsPlacement;
    }
#if defined(__EMSCRIPTEN__)
    return WindowControlsPlacement::None;
#elif defined(__APPLE__)
    return WindowControlsPlacement::Left;
#else
    return WindowControlsPlacement::Right;
#endif
}

void RenderHeaderBarBackground(const Node* node, ICanvas& canvas, const UiTheme& theme) noexcept {
    const auto* payload = node ? node->As<HeaderBarRenderPayload>() : nullptr;
    if (!payload) return;
    const Rect& r = node->computedRect;
    if (r.w <= 0.5f || r.h <= 0.5f) return;

    theme.Painter().DrawHeaderBar(canvas, theme, HeaderBarPaintParams{
        .rect = r,
        .bg = payload->bgColor,
        .explicitBg = payload->explicitBg,
        .borderColor = payload->borderColor,
        .borderWidth = payload->borderWidth,
        .topRadius = payload->topRadius,
        .drawBottomBorder = payload->drawBottomBorder,
        .focused = payload->focused
    });

    if (payload->centerTitle && (!payload->centeredTitle.empty() || !payload->centeredSubtitle.empty())) {
        const HeaderBarChromeMetrics cm = theme.Painter().GetHeaderBarMetrics(theme, payload->focused);
        const Color titleCol = cm.titleColor;
        const Color subCol = cm.subtitleColor;
        const keel::f32 titleFont = theme.ResolveFontSize(payload->centeredTitle, TextRole::LabelLarge, payload->size);
        const keel::f32 subFont = theme.ResolveFontSize(payload->centeredSubtitle, TextRole::LabelSmall, payload->size);

        const keel::f32 icSz = payload->centeredIcon.IsValid() ? keel::min(16.0f, r.h * 0.50f) : 0.0f;
        const keel::f32 titleW = payload->centeredTitle.empty() ? 0.0f : canvas.MeasureText(payload->centeredTitle.text, titleFont).x;
        const keel::f32 subW = payload->centeredSubtitle.empty() ? 0.0f : canvas.MeasureText(payload->centeredSubtitle.text, subFont).x;
        const keel::f32 gap1 = (icSz > 0.0f && titleW > 0.0f) ? 6.0f : 0.0f;
        const keel::f32 gap2 = (titleW > 0.0f && subW > 0.0f) ? 8.0f : 0.0f;

        const keel::f32 controlsReserve =
            cm.controlBtnWidth * 3.0f + cm.controlGap * 2.0f + cm.closeExtraGap + cm.rightEdgePadding + 12.0f;
        const keel::f32 maxCenterW = keel::max(0.0f, r.w - controlsReserve * 2.0f);
        if (maxCenterW > 24.0f) {
            const keel::f32 titleBudget = keel::max(0.0f, maxCenterW - icSz - gap1);
            keel::FixedString<256> titleBuf;
            keel::StringRef drawTitle = payload->centeredTitle.text;
            keel::f32 effTitleW = titleW;
            bool titleTruncated = false;
            if (!drawTitle.empty() && titleW > titleBudget + 0.5f) {
                titleBuf = TextUtil::TruncateWithEllipsis(drawTitle, titleBudget, titleFont, &canvas);
                drawTitle = keel::StringRef(titleBuf);
                effTitleW = drawTitle.empty() ? 0.0f : canvas.MeasureText(drawTitle, titleFont).x;
                titleTruncated = true;
            }

            keel::FixedString<256> subBuf;
            keel::StringRef drawSub{};
            keel::f32 effSubW = 0.0f;
            if (!titleTruncated && !payload->centeredSubtitle.empty()) {
                const keel::f32 subBudget = keel::max(0.0f, titleBudget - effTitleW - gap2);
                if (subBudget >= 24.0f) {
                    drawSub = payload->centeredSubtitle.text;
                    effSubW = subW;
                    if (subW > subBudget + 0.5f) {
                        subBuf = TextUtil::TruncateWithEllipsis(drawSub, subBudget, subFont, &canvas);
                        drawSub = keel::StringRef(subBuf);
                        effSubW = drawSub.empty() ? 0.0f : canvas.MeasureText(drawSub, subFont).x;
                    }
                }
            }

            const keel::f32 effGap1 = (icSz > 0.0f && effTitleW > 0.0f) ? 6.0f : 0.0f;
            const keel::f32 effGap2 = (effTitleW > 0.0f && effSubW > 0.0f) ? 8.0f : 0.0f;
            const keel::f32 drawW = keel::min(icSz + effGap1 + effTitleW + effGap2 + effSubW, maxCenterW);
            keel::f32 cx = keel::round(r.Center().x - drawW * 0.5f);
            const Rect clipR(keel::round(r.Center().x - maxCenterW * 0.5f), r.y, maxCenterW, r.h);
            canvas.PushClip(clipR);
            if (icSz > 0.0f) {
                payload->centeredIcon.Draw(canvas, Rect(cx, r.Center().y - icSz * 0.5f, icSz, icSz), titleCol);
                cx += icSz + effGap1;
            }
            if (!drawTitle.empty()) {
                const keel::f32 avail = keel::max(0.0f, clipR.Right() - cx);
                canvas.DrawTextBox(drawTitle, Rect(cx, r.y, avail, r.h), titleFont, titleCol, 0.0f, 0.5f);
                cx += effTitleW + effGap2;
            }
            if (!drawSub.empty() && cx < clipR.Right() - 4.0f) {
                const keel::f32 avail = keel::max(0.0f, clipR.Right() - cx);
                canvas.DrawTextBox(drawSub, Rect(cx, r.y, avail, r.h), subFont, subCol, 0.0f, 0.5f);
            }
            canvas.PopClip();
        }
    }
}

Vec2 MeasureHeaderTitleBlock(
    const Node* node,
    keel::f32 /*availableWidth*/,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const auto* payload = node ? node->As<HeaderTitleBlockPayload>() : nullptr;
    if (!payload) return {0.0f, 0.0f};

    const keel::f32 titleFont = theme.ResolveFontSize(payload->title, TextRole::LabelLarge, payload->size);
    const keel::f32 subFont = theme.ResolveFontSize(payload->subtitle, TextRole::LabelSmall, payload->size);
    const keel::f32 icSz = payload->icon.IsValid() ? 16.0f : 0.0f;
    const keel::f32 titleW = payload->title.empty() ? 0.0f : TextUtil::MeasureWidth(payload->title, titleFont, canvas);
    const keel::f32 subW = payload->subtitle.empty() ? 0.0f : TextUtil::MeasureWidth(payload->subtitle, subFont, canvas);

    keel::f32 w = 4.0f;
    if (icSz > 0.0f) w += icSz;
    if (titleW > 0.0f) w += (icSz > 0.0f ? 6.0f : 0.0f) + titleW;
    if (subW > 0.0f) w += ((icSz > 0.0f || titleW > 0.0f) ? 8.0f : 0.0f) + subW;
    w += 4.0f;
    const keel::f32 h = keel::max(18.0f, titleFont + 6.0f);
    return {keel::ceil(w), h};
}

void RenderHeaderTitleBlock(const Node* node, ICanvas& canvas, const UiTheme& theme) noexcept {
    const auto* payload = node ? node->As<HeaderTitleBlockPayload>() : nullptr;
    if (!payload) return;
    const Rect& r = node->computedRect;
    if (r.w <= 1.0f || r.h <= 1.0f) return;

    const HeaderBarChromeMetrics cm = theme.Painter().GetHeaderBarMetrics(theme, payload->focused);
    const Color titleCol = cm.titleColor;
    const Color subCol = cm.subtitleColor;
    const keel::f32 titleFont = theme.ResolveFontSize(payload->title, TextRole::LabelLarge, payload->size);
    const keel::f32 subFont = theme.ResolveFontSize(payload->subtitle, TextRole::LabelSmall, payload->size);

    canvas.PushClip(r);
    keel::f32 cx = r.x + 4.0f;
    if (payload->icon.IsValid()) {
        const keel::f32 icSz = keel::min(16.0f, r.h * 0.72f);
        payload->icon.Draw(canvas, Rect(cx, r.Center().y - icSz * 0.5f, icSz, icSz), titleCol);
        cx += icSz + 6.0f;
    }
    bool titleTruncated = false;
    if (!payload->title.empty() && cx < r.Right() - 4.0f) {
        const keel::f32 tw = canvas.MeasureText(payload->title.text, titleFont).x;
        const keel::f32 avail = keel::max(0.0f, r.Right() - cx - 4.0f);
        if (tw > avail + 0.5f) {
            const auto trunc = TextUtil::TruncateWithEllipsis(payload->title.text, avail, titleFont, &canvas);
            if (!trunc.empty()) {
                canvas.DrawTextBox(keel::StringRef(trunc), Rect(cx, r.y, avail, r.h), titleFont, titleCol, 0.0f, 0.5f);
            }
            titleTruncated = true;
        } else {
            canvas.DrawTextBox(payload->title.text, Rect(cx, r.y, avail, r.h), titleFont, titleCol, 0.0f, 0.5f);
            cx += tw + 8.0f;
        }
    }
    if (!titleTruncated && !payload->subtitle.empty() && cx < r.Right() - 24.0f) {
        const keel::f32 sw = canvas.MeasureText(payload->subtitle.text, subFont).x;
        const keel::f32 avail = keel::max(0.0f, r.Right() - cx - 4.0f);
        if (sw > avail + 0.5f) {
            const auto trunc = TextUtil::TruncateWithEllipsis(payload->subtitle.text, avail, subFont, &canvas);
            if (!trunc.empty()) {
                canvas.DrawTextBox(keel::StringRef(trunc), Rect(cx, r.y, avail, r.h), subFont, subCol, 0.0f, 0.5f);
            }
        } else {
            canvas.DrawTextBox(payload->subtitle.text, Rect(cx, r.y, avail, r.h), subFont, subCol, 0.0f, 0.5f);
        }
    }
    canvas.PopClip();
}

Vec2 MeasureWinCtrlButton(
    const Node* node,
    keel::f32 /*availableWidth*/,
    ICanvas* /*canvas*/,
    const UiTheme& /*theme*/
) noexcept {
    const auto* p = node ? node->As<WinCtrlButtonPayload>() : nullptr;
    if (p) {
        return {p->width, p->height};
    }
    return {38.0f, 28.0f};
}

void RenderWinCtrlButton(const Node* node, ICanvas& canvas, const UiTheme& theme) noexcept {
    const auto* p = node ? node->As<WinCtrlButtonPayload>() : nullptr;
    if (!p) return;
    theme.Painter().DrawWindowControlButton(canvas, theme, WindowControlPaintParams{
        .rect = node->computedRect,
        .kind = p->kind,
        .maximized = p->maximized,
        .focused = p->focused,
        .macStyle = p->macStyle,
        .hovered = p->hovered,
        .pressed = p->pressed,
        .disabled = p->disabled
    });
}

struct WinCtrlHit {
    bool hovered{false};
    bool pressed{false};
    bool clicked{false};
};

[[nodiscard]] WinCtrlHit EvalWinCtrlBtn(Context& ctx, Id id, Rect rect, bool disabled) noexcept {
    WinCtrlHit out{};
    if (disabled || rect.w <= 0.5f || rect.h <= 0.5f) {
        return out;
    }
    const Vec2 mouse = ctx.GetMousePos();
    const bool occluded = ctx.IsOverlayOccluded(id, mouse, OverlayLayer::Panel);
    const bool inside = !occluded && rect.Contains(mouse);
    const Id active = ctx.GetActiveId();

    out.hovered = inside && (!active.IsValid() || active == id);
    if (inside && ctx.IsMouseClicked() && !active.IsValid()) {
        ctx.SetActiveId(id);
        ctx.ConsumeMouseClick();
    }
    const bool isActive = (ctx.GetActiveId() == id);
    out.pressed = isActive && ctx.IsMouseDown() && inside;
    if (isActive && ctx.IsMouseReleased()) {
        if (inside) {
            out.clicked = true;
        }
        ctx.SetActiveId(Id::Null());
    }
    return out;
}

void EmitWindowControlButton(
    Context& ctx,
    Id btnId,
    WindowControlKind kind,
    WindowHitRegion hitRegion,
    bool bindToWindow,
    bool maximized,
    bool focused,
    bool macStyle,
    bool disabled,
    bool hovered,
    bool pressed,
    keel::f32 btnW,
    keel::f32 btnH
) {
    if (hovered && !disabled) {
        ctx.SetCursor(CursorType::Pointer);
    }

    Node* btn = ctx.CreateNode(NodeType::Button, btnId, disabled);
    btn->widthMode = SizeMode::Fixed;
    btn->heightMode = SizeMode::Fixed;
    btn->fixedWidth = btnW;
    btn->fixedHeight = btnH;
    btn->hovered = hovered;
    btn->pressed = pressed;

    auto* payload = ctx.AllocPayload<WinCtrlButtonPayload>(WinCtrlButtonPayload{
        .kind = kind,
        .maximized = maximized,
        .focused = focused,
        .macStyle = macStyle,
        .hovered = hovered,
        .pressed = pressed,
        .disabled = disabled,
        .width = btnW,
        .height = btnH
    });
    btn->SetPayload(payload);
    btn->measureFn = &MeasureWinCtrlButton;
    btn->renderFn = &RenderWinCtrlButton;
    btn->visualHash = detail::HashArgs(
        keel::as<keel::u8>(kind),
        maximized,
        focused,
        macStyle,
        hovered,
        pressed,
        disabled,
        btnW,
        btnH
    );

    if (bindToWindow) {
        ctx.RegisterWindowControlNode(hitRegion, btnId);
    }
}

void EmitControlsGroup(
    Context& ctx,
    const HeaderBarOptions& opts,
    WindowControlsPlacement placement,
    bool maximized,
    bool focused,
    Id minBtnId,
    Id maxBtnId,
    Id clsBtnId,
    bool minHovered,
    bool minPressed,
    bool maxHovered,
    bool maxPressed,
    bool clsHovered,
    bool clsPressed,
    keel::f32 barHeight
) {
    if (placement == WindowControlsPlacement::None) {
        return;
    }
    const UiTheme& theme = ctx.GetTheme();
    const HeaderBarChromeMetrics cm = theme.Painter().GetHeaderBarMetrics(theme, focused);
    const bool macStyle = (placement == WindowControlsPlacement::Left);
    const keel::f32 btnW = macStyle ? 20.0f : cm.controlBtnWidth;
    const keel::f32 btnH = macStyle
        ? 20.0f
        : ((cm.controlBtnHeight > 0.0f) ? cm.controlBtnHeight : keel::max(24.0f, barHeight - 6.0f));

    if (macStyle) {
        // macOS traffic-light order: Close, Minimize, Maximize
        EmitWindowControlButton(ctx, clsBtnId, WindowControlKind::Close, WindowHitRegion::CloseButton, opts.bindToWindow, maximized, focused, true, opts.disabled, clsHovered, clsPressed, btnW, btnH);
        EmitWindowControlButton(ctx, minBtnId, WindowControlKind::Minimize, WindowHitRegion::MinimizeButton, opts.bindToWindow, maximized, focused, true, opts.disabled, minHovered, minPressed, btnW, btnH);
        EmitWindowControlButton(ctx, maxBtnId, WindowControlKind::Maximize, WindowHitRegion::MaximizeButton, opts.bindToWindow, maximized, focused, true, opts.disabled, maxHovered, maxPressed, btnW, btnH);
        return;
    }

    // Wrap right-edge controls in a Fixed-width Row so window controls never shrink when title overflows
    const Id grpId(detail::HashArgs(minBtnId.value, keel::u64(0x48425F475250ull)));
    Node* grp = ctx.CreateNode(NodeType::Row, grpId, opts.disabled);
    grp->direction = FlexDirection::Row;
    grp->widthMode = SizeMode::Fixed;
    grp->fixedWidth = btnW * 3.0f + cm.controlGap * 2.0f + cm.closeExtraGap;
    grp->heightMode = SizeMode::Fixed;
    grp->fixedHeight = btnH;
    grp->align = Alignment::Center;
    grp->gap = cm.controlGap;
    ctx.FinishContainer(grp);
    ContainerScope grpScope(&ctx, grp);

    EmitWindowControlButton(ctx, minBtnId, WindowControlKind::Minimize, WindowHitRegion::MinimizeButton, opts.bindToWindow, maximized, focused, false, opts.disabled, minHovered, minPressed, btnW, btnH);
    EmitWindowControlButton(ctx, maxBtnId, WindowControlKind::Maximize, WindowHitRegion::MaximizeButton, opts.bindToWindow, maximized, focused, false, opts.disabled, maxHovered, maxPressed, btnW, btnH);
    if (cm.closeExtraGap > 0.0f) {
        const Id gapId(detail::HashArgs(clsBtnId.value, keel::u64(0x48425F474150ull)));
        Node* gapNode = ctx.CreateNode(NodeType::Spacer, gapId);
        gapNode->widthMode = SizeMode::Fixed;
        gapNode->heightMode = SizeMode::Fixed;
        gapNode->fixedWidth = cm.closeExtraGap;
        gapNode->fixedHeight = 1.0f;
        gapNode->isSpacer = true;
    }
    EmitWindowControlButton(ctx, clsBtnId, WindowControlKind::Close, WindowHitRegion::CloseButton, opts.bindToWindow, maximized, focused, false, opts.disabled, clsHovered, clsPressed, btnW, btnH);
}

} // namespace

void HeaderBarScope::CloseHeaderBar() noexcept {
    if (!m_activeHeader || !m_ctx) {
        return;
    }
    if (m_placement == WindowControlsPlacement::Right) {
        // Ensure right-edge window controls stay pinned to the right even if caller omitted hb.Spacer()
        bool hasStretchChild = false;
        if (m_node) {
            for (const Node* ch = m_node->firstChild; ch != nullptr; ch = ch->nextSibling) {
                if (ch->widthMode == SizeMode::Stretch || ch->isSpacer) {
                    hasStretchChild = true;
                    break;
                }
            }
        }
        if (!hasStretchChild) {
            Spacer();
        }
        detail::HeaderBar::EmitTrailingControls(
            *m_ctx,
            m_opts,
            m_placement,
            m_maximized,
            m_focused,
            m_minBtnId,
            m_maxBtnId,
            m_clsBtnId,
            m_minHovered,
            m_minPressed,
            m_maxHovered,
            m_maxPressed,
            m_clsHovered,
            m_clsPressed
        );
    }
    m_activeHeader = false;
    ContainerScope::Close();
}

void HeaderBarScope::Spacer(keel::SourceLocation loc) {
    if (!m_ctx) return;
    const Id id = m_ctx->MakeChildId(loc);
    Node* spacer = m_ctx->CreateNode(NodeType::Spacer, id);
    spacer->widthMode = SizeMode::Stretch;
    spacer->isSpacer = true;
    spacer->flexGrow = 1.0f;
    spacer->measureFn = [](const Node*, keel::f32 /*availableWidth*/, ICanvas*, const UiTheme&) noexcept -> Vec2 {
        return {0.0f, 0.0f};
    };
}

void HeaderBarScope::Gap(keel::f32 sizeDp, keel::SourceLocation loc) {
    if (!m_ctx) return;
    guikit::detail::Spacer::Gap(*m_ctx, sizeDp, loc);
}

void HeaderBarScope::Separator(keel::f32 heightRatio, keel::SourceLocation loc) {
    if (!m_ctx || !m_node) return;
    guikit::detail::Separator::Run(*m_ctx, heightRatio, m_ctx->GetTheme().palette.borderSubtle, loc);
}

void detail::HeaderBar::EmitTrailingControls(
    Context& ctx,
    const HeaderBarOptions& opts,
    WindowControlsPlacement placement,
    bool maximized,
    bool focused,
    Id minBtnId,
    Id maxBtnId,
    Id clsBtnId,
    bool minHovered,
    bool minPressed,
    bool maxHovered,
    bool maxPressed,
    bool clsHovered,
    bool clsPressed
) {
    const UiTheme& theme = ctx.GetTheme();
    const keel::f32 barH = opts.height.value_or(ResolveHeaderBarHeight(theme, opts.size, focused));
    EmitControlsGroup(
        ctx, opts, placement, maximized, focused,
        minBtnId, maxBtnId, clsBtnId,
        minHovered, minPressed, maxHovered, maxPressed, clsHovered, clsPressed,
        barH
    );
}

HeaderBarScope detail::HeaderBar::Open(
    Context& ctx,
    const HeaderBarOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const Id minBtnId(detail::HashArgs(id.value, keel::u64(0x48425F4D494Eull)));
    const Id maxBtnId(detail::HashArgs(id.value, keel::u64(0x48425F4D4158ull)));
    const Id clsBtnId(detail::HashArgs(id.value, keel::u64(0x48425F434C53ull)));
    const Id titleNodeId(detail::HashArgs(id.value, keel::u64(0x48425F54544Cull)));

    Node* node = ctx.CreateNode(NodeType::HeaderBar, id, opts.disabled);
    const UiTheme& theme = node->GetTheme(ctx.GetTheme());
    const bool activeDisabled = node->disabled;
    const WindowHostService& host = ctx.GetWindowHostService();

    const bool maximized = opts.maximizedOverride.has_value()
        ? *opts.maximizedOverride
        : ((opts.bindToWindow && host.isMaximized) ? host.isMaximized() : false);
    const bool focused = opts.focusedOverride.has_value()
        ? *opts.focusedOverride
        : ((opts.bindToWindow && host.isFocused) ? host.isFocused() : true);
    const WindowControlsPlacement placement = ResolveControlsPlacement(opts);
    const HeaderBarChromeMetrics cm = theme.Painter().GetHeaderBarMetrics(theme, focused);

    // Evaluate each window control button ONCE per frame (avoids double-click consumption)
    WinCtrlHit minHit{};
    WinCtrlHit maxHit{};
    WinCtrlHit clsHit{};
    if (placement != WindowControlsPlacement::None) {
        minHit = EvalWinCtrlBtn(ctx, minBtnId, ctx.GetPrevFrameBounds(minBtnId), activeDisabled);
        maxHit = EvalWinCtrlBtn(ctx, maxBtnId, ctx.GetPrevFrameBounds(maxBtnId), activeDisabled);
        clsHit = EvalWinCtrlBtn(ctx, clsBtnId, ctx.GetPrevFrameBounds(clsBtnId), activeDisabled);
        if (minHit.clicked && opts.bindToWindow && host.minimize) {
            host.minimize();
        }
        if (maxHit.clicked && opts.bindToWindow) {
            if (maximized) {
                if (host.restore) host.restore();
            } else {
                if (host.maximize) host.maximize();
            }
        }
        if (clsHit.clicked && opts.bindToWindow && host.requestClose) {
            host.requestClose();
        }
    }

    // Fallback interaction when OS does not perform native WM_NCHITTEST (e.g. Linux X11):
    // double-click on free caption space toggles maximize/restore; mouse-down initiates system move.
    const Rect prevBarRect = ctx.GetPrevFrameBounds(id);
    if (opts.bindToWindow && !activeDisabled && prevBarRect.w > 1.0f && prevBarRect.h > 1.0f) {
        const Vec2 mouse = ctx.GetMousePos();
        if (prevBarRect.Contains(mouse)) {
            const WindowHitRegion reg = ctx.ResolveWindowHitRegion(mouse);
            if (reg == WindowHitRegion::Caption) {
                if (opts.allowDoubleClickMaximize && ctx.IsMouseDoubleClicked()) {
                    if (maximized) {
                        if (host.restore) host.restore();
                    } else {
                        if (host.maximize) host.maximize();
                    }
                    ctx.ConsumeMouseClick();
                } else if (opts.allowDrag && ctx.IsMouseClicked() && host.startSystemMove) {
                    host.startSystemMove();
                    ctx.ConsumeMouseClick();
                }
            }
        }
    }

    const keel::f32 barH = opts.height.value_or(ResolveHeaderBarHeight(theme, opts.size, focused));
    const keel::f32 gap = opts.gap.value_or(6.0f);
    const Padding pad = opts.padding.value_or(Padding(
        8.0f,
        0.0f,
        (placement == WindowControlsPlacement::Right ? cm.rightEdgePadding : 8.0f),
        0.0f
    ));

    node->direction = FlexDirection::Row;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Fixed;
    node->fixedHeight = barH;
    node->align = Alignment::Center;
    node->gap = gap;
    node->padding = pad;
    node->clipChildren = true;
    node->uiSize = opts.size;
    node->active = focused;
    node->SetSurfaceRole(SurfaceRole::None);
    node->backgroundColor = Color::Transparent();
    node->borderColor = Color::Transparent();
    node->borderWidth = 0.0f;

    const bool explicitBg = opts.background.HasAny();
    const Color defaultBg = focused ? theme.palette.surfaceElevated : theme.palette.surface;
    const Color resolvedBg = opts.background.IsBackgroundEnabled(false)
        ? theme.ResolveColor(opts.background, false, false, activeDisabled, defaultBg)
        : defaultBg;
    const Color resolvedBorder = (opts.border.width > 0.0f || opts.border.color.HasAny())
        ? theme.ResolveColor(opts.border.color, false, false, activeDisabled, theme.palette.borderSubtle)
        : Color::Transparent();

    // Resolve displayed title (fall back to host window title when empty and showWindowTitle is enabled)
    UiText resolvedTitle = opts.title;
    if (resolvedTitle.empty() && opts.showWindowTitle && opts.bindToWindow && host.getTitle) {
        const keel::String hostTitle = host.getTitle();
        if (!hostTitle.empty()) {
            resolvedTitle = UiText(ctx.AllocString(hostTitle.as_ref()));
        }
    }
    const UiText allocTitle = ctx.AllocText(resolvedTitle);
    const UiText allocSub = ctx.AllocText(opts.subtitle);
    const Icon allocIcon = ctx.AllocIcon(opts.icon);

    const keel::f32 inheritedTopRadius = opts.border.radius.has_value()
        ? *opts.border.radius
        : ((!opts.bindToWindow && node->parent != nullptr && node->parent->cornerRadius > 0.5f)
            ? keel::max(0.0f, node->parent->cornerRadius - node->parent->borderWidth * 0.5f)
            : 0.0f);

    auto* renderPayload = ctx.AllocPayload<HeaderBarRenderPayload>(HeaderBarRenderPayload{
        .centeredTitle = allocTitle,
        .centeredSubtitle = allocSub,
        .centeredIcon = allocIcon,
        .centerTitle = opts.centerTitle,
        .focused = focused,
        .explicitBg = explicitBg,
        .bgColor = resolvedBg,
        .drawBottomBorder = (opts.border.width > 0.0f && resolvedBorder.a > 0u),
        .borderColor = resolvedBorder,
        .borderWidth = opts.border.width,
        .topRadius = inheritedTopRadius,
        .size = opts.size
    });
    node->SetPayload(renderPayload);
    node->renderFn = &RenderHeaderBarBackground;
    node->visualHash = detail::HashArgs(
        barH,
        resolvedBg.ToUint32(),
        explicitBg,
        resolvedBorder.ToUint32(),
        opts.border.width,
        inheritedTopRadius,
        opts.centerTitle,
        maximized,
        focused,
        cm.accentCaption,
        cm.titleColor.ToUint32(),
        keel::as<keel::u8>(placement),
        allocTitle.text,
        allocSub.text
    );

    if (opts.bindToWindow && opts.allowDrag) {
        ctx.RegisterWindowDragNode(id);
    }

    ctx.FinishContainer(node);

    // Emit left-edge window controls (macOS traffic-light placement) before title & user children
    if (placement == WindowControlsPlacement::Left) {
        EmitControlsGroup(
            ctx, opts, placement, maximized, focused,
            minBtnId, maxBtnId, clsBtnId,
            minHit.hovered, minHit.pressed,
            maxHit.hovered, maxHit.pressed,
            clsHit.hovered, clsHit.pressed,
            barH
        );
    }

    // Emit inline leading [Icon] [Title] [Subtitle] block when not center-aligned
    if (!opts.centerTitle && (allocIcon.IsValid() || !allocTitle.empty() || !allocSub.empty())) {
        Node* titleNode = ctx.CreateNode(NodeType::Text, titleNodeId, activeDisabled);
        titleNode->widthMode = SizeMode::Fit;
        titleNode->heightMode = SizeMode::Fit;
        titleNode->uiSize = opts.size;
        auto* tp = ctx.AllocPayload<HeaderTitleBlockPayload>(HeaderTitleBlockPayload{
            .title = allocTitle,
            .subtitle = allocSub,
            .icon = allocIcon,
            .focused = focused,
            .size = opts.size
        });
        titleNode->SetPayload(tp);
        titleNode->measureFn = &MeasureHeaderTitleBlock;
        titleNode->renderFn = &RenderHeaderTitleBlock;
        titleNode->visualHash = detail::HashArgs(
            allocTitle.text,
            allocSub.text,
            focused,
            cm.titleColor.ToUint32(),
            allocIcon.IsValid()
        );
    }

    Response res{};
    res.id = id;
    res.rect = prevBarRect;
    res.hovered = prevBarRect.Contains(ctx.GetMousePos());
    res.ctx = &ctx;

    return HeaderBarScope(
        &ctx,
        node,
        res,
        opts,
        placement,
        maximized,
        focused,
        minHit.clicked,
        maxHit.clicked,
        clsHit.clicked,
        minBtnId,
        maxBtnId,
        clsBtnId,
        minHit.hovered,
        minHit.pressed,
        maxHit.hovered,
        maxHit.pressed,
        clsHit.hovered,
        clsHit.pressed
    );
}

} // namespace guikit
