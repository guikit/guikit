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

enum class WinCtrlKind : keel::u8 {
    Minimize = 0,
    Maximize,
    Close
};

struct HeaderBarRenderPayload {
    UiText centeredTitle{};
    UiText centeredSubtitle{};
    Icon centeredIcon{};
    bool centerTitle{false};
    bool focused{true};
    bool drawBottomBorder{true};
    Color borderColor{};
    keel::f32 borderWidth{1.0f};
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
    WinCtrlKind kind{WinCtrlKind::Minimize};
    bool maximized{false};
    bool focused{true};
    bool macStyle{false};
    bool hovered{false};
    bool pressed{false};
    bool disabled{false};
};

[[nodiscard]] keel::f32 ResolveHeaderBarHeight(const UiTheme& theme, UiSize size) noexcept {
    switch (size.GetPreset()) {
        case UiSize::Preset::Xs: return 30.0f;
        case UiSize::Preset::Sm: return 34.0f;
        case UiSize::Preset::Lg: return 44.0f;
        case UiSize::Preset::Xl: return 48.0f;
        case UiSize::Preset::Md:
        case UiSize::Preset::None:
            return 38.0f;
        default:
            return keel::round(38.0f * theme.SizeFactor(size));
    }
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

void DrawMinimizeGlyph(ICanvas& canvas, Rect r, Color col) noexcept {
    const keel::f32 y = keel::round(r.Center().y) + 0.5f;
    canvas.DrawLine(Vec2(r.x, y), Vec2(r.Right(), y), col, 1.25f);
}

void DrawMaximizeGlyph(ICanvas& canvas, Rect r, Color col) noexcept {
    const Rect sq(keel::round(r.x) + 0.5f, keel::round(r.y) + 0.5f, keel::max(4.0f, keel::round(r.w) - 1.0f), keel::max(4.0f, keel::round(r.h) - 1.0f));
    canvas.DrawRoundRect(sq, 1.0f, Color::Transparent(), col, 1.25f);
}

void DrawRestoreGlyph(ICanvas& canvas, Rect r, Color col) noexcept {
    const keel::f32 sz = keel::max(4.0f, keel::round(r.w * 0.78f));
    const Rect front(keel::round(r.x) + 0.5f, keel::round(r.Bottom() - sz) + 0.5f, sz - 1.0f, sz - 1.0f);
    canvas.DrawRoundRect(front, 1.0f, Color::Transparent(), col, 1.2f);
    const keel::f32 bx1 = keel::round(r.Right() - sz) + 0.5f;
    const keel::f32 by1 = keel::round(r.y) + 0.5f;
    const keel::f32 bx2 = keel::round(r.Right()) - 0.5f;
    const keel::f32 by2 = by1 + sz - 1.0f;
    canvas.DrawLine(Vec2(bx1, by1), Vec2(bx2, by1), col, 1.2f);
    canvas.DrawLine(Vec2(bx2, by1), Vec2(bx2, by2), col, 1.2f);
    canvas.DrawLine(Vec2(bx1, by1), Vec2(bx1, front.y), col, 1.2f);
    canvas.DrawLine(Vec2(front.Right(), by2), Vec2(bx2, by2), col, 1.2f);
}

void DrawCloseGlyph(ICanvas& canvas, Rect r, Color col) noexcept {
    const keel::f32 x0 = keel::round(r.x) + 0.5f;
    const keel::f32 y0 = keel::round(r.y) + 0.5f;
    const keel::f32 x1 = keel::round(r.Right()) - 0.5f;
    const keel::f32 y1 = keel::round(r.Bottom()) - 0.5f;
    canvas.DrawLine(Vec2(x0, y0), Vec2(x1, y1), col, 1.3f);
    canvas.DrawLine(Vec2(x1, y0), Vec2(x0, y1), col, 1.3f);
}

void RenderHeaderBarBackground(const Node* node, ICanvas& canvas, const UiTheme& theme) noexcept {
    const auto* payload = node ? node->As<HeaderBarRenderPayload>() : nullptr;
    if (!payload) return;
    const Rect& r = node->computedRect;
    if (r.w <= 0.5f || r.h <= 0.5f) return;

    if (payload->drawBottomBorder && payload->borderWidth > 0.0f && payload->borderColor.a > 0u) {
        const keel::f32 y = r.Bottom() - payload->borderWidth * 0.5f;
        theme.Painter().DrawDivider(canvas, theme, Vec2(r.x, y), Vec2(r.Right(), y), payload->borderColor, payload->borderWidth);
    }

    if (payload->centerTitle && (!payload->centeredTitle.empty() || !payload->centeredSubtitle.empty())) {
        const auto& pal = theme.palette;
        const Color titleCol = payload->focused ? pal.textPrimary : pal.textSecondary;
        const Color subCol = payload->focused ? pal.textSecondary : pal.textMuted;
        const keel::f32 titleFont = theme.ResolveFontSize(payload->centeredTitle, TextRole::LabelLarge, payload->size);
        const keel::f32 subFont = theme.ResolveFontSize(payload->centeredSubtitle, TextRole::LabelSmall, payload->size);

        const keel::f32 icSz = payload->centeredIcon.IsValid() ? keel::min(16.0f, r.h * 0.45f) : 0.0f;
        const keel::f32 titleW = payload->centeredTitle.empty() ? 0.0f : canvas.MeasureText(payload->centeredTitle.text, titleFont).x;
        const keel::f32 subW = payload->centeredSubtitle.empty() ? 0.0f : canvas.MeasureText(payload->centeredSubtitle.text, subFont).x;
        const keel::f32 gap1 = (icSz > 0.0f && titleW > 0.0f) ? 6.0f : 0.0f;
        const keel::f32 gap2 = (titleW > 0.0f && subW > 0.0f) ? 8.0f : 0.0f;
        const keel::f32 totalW = icSz + gap1 + titleW + gap2 + subW;

        const keel::f32 maxCenterW = keel::max(0.0f, r.w - 160.0f);
        if (maxCenterW > 24.0f) {
            const keel::f32 drawW = keel::min(totalW, maxCenterW);
            keel::f32 cx = keel::round(r.Center().x - drawW * 0.5f);
            const Rect clipR(keel::round(r.Center().x - maxCenterW * 0.5f), r.y, maxCenterW, r.h);
            canvas.PushClip(clipR);
            if (icSz > 0.0f) {
                payload->centeredIcon.Draw(canvas, Rect(cx, r.Center().y - icSz * 0.5f, icSz, icSz), titleCol);
                cx += icSz + gap1;
            }
            if (!payload->centeredTitle.empty()) {
                const keel::f32 avail = keel::max(0.0f, clipR.Right() - cx);
                canvas.DrawTextBox(payload->centeredTitle.text, Rect(cx, r.y, avail, r.h), titleFont, titleCol, 0.0f, 0.5f);
                cx += titleW + gap2;
            }
            if (!payload->centeredSubtitle.empty() && cx < clipR.Right() - 12.0f) {
                const keel::f32 avail = keel::max(0.0f, clipR.Right() - cx);
                canvas.DrawTextBox(payload->centeredSubtitle.text, Rect(cx, r.y, avail, r.h), subFont, subCol, 0.0f, 0.5f);
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
    const keel::f32 h = keel::max(20.0f, titleFont + 8.0f);
    return {keel::ceil(w), h};
}

void RenderHeaderTitleBlock(const Node* node, ICanvas& canvas, const UiTheme& theme) noexcept {
    const auto* payload = node ? node->As<HeaderTitleBlockPayload>() : nullptr;
    if (!payload) return;
    const Rect& r = node->computedRect;
    if (r.w <= 1.0f || r.h <= 1.0f) return;

    const auto& pal = theme.palette;
    const Color titleCol = payload->focused ? pal.textPrimary : pal.textSecondary;
    const Color subCol = payload->focused ? pal.textSecondary : pal.textMuted;
    const keel::f32 titleFont = theme.ResolveFontSize(payload->title, TextRole::LabelLarge, payload->size);
    const keel::f32 subFont = theme.ResolveFontSize(payload->subtitle, TextRole::LabelSmall, payload->size);

    keel::f32 cx = r.x + 4.0f;
    if (payload->icon.IsValid()) {
        const keel::f32 icSz = keel::min(16.0f, r.h * 0.72f);
        payload->icon.Draw(canvas, Rect(cx, r.Center().y - icSz * 0.5f, icSz, icSz), titleCol);
        cx += icSz + 6.0f;
    }
    if (!payload->title.empty()) {
        const keel::f32 tw = canvas.MeasureText(payload->title.text, titleFont).x;
        const keel::f32 avail = keel::max(0.0f, r.Right() - cx - 4.0f);
        canvas.DrawTextBox(payload->title.text, Rect(cx, r.y, avail, r.h), titleFont, titleCol, 0.0f, 0.5f);
        cx += tw + 8.0f;
    }
    if (!payload->subtitle.empty() && cx < r.Right() - 10.0f) {
        const keel::f32 avail = keel::max(0.0f, r.Right() - cx - 4.0f);
        canvas.DrawTextBox(payload->subtitle.text, Rect(cx, r.y, avail, r.h), subFont, subCol, 0.0f, 0.5f);
    }
}

Vec2 MeasureWinCtrlButton(
    const Node* node,
    keel::f32 /*availableWidth*/,
    ICanvas* /*canvas*/,
    const UiTheme& /*theme*/
) noexcept {
    const auto* p = node ? node->As<WinCtrlButtonPayload>() : nullptr;
    if (p && p->macStyle) {
        return {20.0f, 20.0f};
    }
    return {38.0f, 28.0f};
}

void RenderWinCtrlButton(const Node* node, ICanvas& canvas, const UiTheme& theme) noexcept {
    const auto* p = node ? node->As<WinCtrlButtonPayload>() : nullptr;
    if (!p) return;
    const Rect& r = node->computedRect;
    if (r.w <= 1.0f || r.h <= 1.0f) return;

    const auto& pal = theme.palette;

    if (p->macStyle) {
        const keel::f32 d = keel::min(12.0f, keel::min(r.w, r.h) - 4.0f);
        const Rect dot(r.Center().x - d * 0.5f, r.Center().y - d * 0.5f, d, d);
        UiVariant var = UiVariant::Danger;
        if (p->kind == WinCtrlKind::Minimize) var = UiVariant::Warning;
        else if (p->kind == WinCtrlKind::Maximize) var = UiVariant::Success;

        const Color semantic = theme.GetSemanticAccent(var);
        const Color fill = p->disabled
            ? pal.surfaceHover
            : (p->focused || p->hovered || p->pressed
                ? (p->pressed ? semantic.WithAlphaF(0.80f) : semantic)
                : pal.border);
        canvas.DrawRoundRect(dot, d * 0.5f, fill, pal.borderSubtle.WithAlphaF(0.5f), 0.75f);

        if ((p->hovered || p->pressed) && !p->disabled) {
            const keel::f32 g = d * 0.48f;
            const Rect gr(dot.Center().x - g * 0.5f, dot.Center().y - g * 0.5f, g, g);
            const Color ink = theme.EnsureReadableInk(pal.background, fill);
            if (p->kind == WinCtrlKind::Minimize) DrawMinimizeGlyph(canvas, gr, ink);
            else if (p->kind == WinCtrlKind::Maximize) DrawMaximizeGlyph(canvas, gr, ink);
            else DrawCloseGlyph(canvas, gr, ink);
        }
        return;
    }

    // Standard Windows / Linux caption button (rendered strictly through IStylePainter)
    Color bg = Color::Transparent();
    Color fg = p->disabled
        ? pal.textDisabled
        : (p->focused ? pal.textSecondary : pal.textMuted);

    if (!p->disabled) {
        if (p->kind == WinCtrlKind::Close) {
            if (p->hovered || p->pressed) {
                const Color dangerBg = theme.GetSemanticAccent(UiVariant::Danger);
                bg = p->pressed ? dangerBg.WithAlphaF(0.85f) : dangerBg;
                fg = theme.EnsureReadableInk(pal.accentText, dangerBg);
            }
        } else {
            if (p->pressed) {
                bg = pal.surfaceActive;
                fg = pal.textPrimary;
            } else if (p->hovered) {
                bg = pal.surfaceHover;
                fg = pal.textPrimary;
            }
        }
    }

    if (bg.a > 0u || (!p->disabled && (p->hovered || p->pressed))) {
        theme.Painter().DrawFrame(canvas, theme, FrameKind::ToolbarButton, FramePaintParams{
            .rect = r,
            .radius = keel::min(4.0f, theme.GetRadius(UiSize::Sm)),
            .bg = bg,
            .hovered = p->hovered,
            .pressed = p->pressed,
            .disabled = p->disabled,
            .accent = (p->kind == WinCtrlKind::Close) ? theme.GetSemanticAccent(UiVariant::Danger) : pal.accent
        });
    }

    const keel::f32 glyphSz = 10.0f;
    const Rect gr(r.Center().x - glyphSz * 0.5f, r.Center().y - glyphSz * 0.5f, glyphSz, glyphSz);
    switch (p->kind) {
        case WinCtrlKind::Minimize:
            DrawMinimizeGlyph(canvas, gr, fg);
            break;
        case WinCtrlKind::Maximize:
            if (p->maximized) {
                DrawRestoreGlyph(canvas, gr, fg);
            } else {
                DrawMaximizeGlyph(canvas, gr, fg);
            }
            break;
        case WinCtrlKind::Close:
            DrawCloseGlyph(canvas, gr, fg);
            break;
    }
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
    WinCtrlKind kind,
    WindowHitRegion hitRegion,
    bool bindToWindow,
    bool maximized,
    bool focused,
    bool macStyle,
    bool disabled,
    bool hovered,
    bool pressed,
    keel::f32 barHeight
) {
    if (hovered && !disabled) {
        ctx.SetCursor(CursorType::Pointer);
    }

    Node* btn = ctx.CreateNode(NodeType::Button, btnId, disabled);
    btn->widthMode = SizeMode::Fixed;
    btn->heightMode = SizeMode::Fixed;
    btn->fixedWidth = macStyle ? 20.0f : 38.0f;
    btn->fixedHeight = macStyle ? 20.0f : keel::max(24.0f, barHeight - 6.0f);
    btn->hovered = hovered;
    btn->pressed = pressed;

    auto* payload = ctx.AllocPayload<WinCtrlButtonPayload>(WinCtrlButtonPayload{
        .kind = kind,
        .maximized = maximized,
        .focused = focused,
        .macStyle = macStyle,
        .hovered = hovered,
        .pressed = pressed,
        .disabled = disabled
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
        disabled
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
    const bool macStyle = (placement == WindowControlsPlacement::Left);
    if (macStyle) {
        // macOS traffic-light order: Close, Minimize, Maximize
        EmitWindowControlButton(ctx, clsBtnId, WinCtrlKind::Close, WindowHitRegion::CloseButton, opts.bindToWindow, maximized, focused, true, opts.disabled, clsHovered, clsPressed, barHeight);
        EmitWindowControlButton(ctx, minBtnId, WinCtrlKind::Minimize, WindowHitRegion::MinimizeButton, opts.bindToWindow, maximized, focused, true, opts.disabled, minHovered, minPressed, barHeight);
        EmitWindowControlButton(ctx, maxBtnId, WinCtrlKind::Maximize, WindowHitRegion::MaximizeButton, opts.bindToWindow, maximized, focused, true, opts.disabled, maxHovered, maxPressed, barHeight);
    } else {
        // Windows / Linux order: Minimize, Maximize/Restore, Close
        EmitWindowControlButton(ctx, minBtnId, WinCtrlKind::Minimize, WindowHitRegion::MinimizeButton, opts.bindToWindow, maximized, focused, false, opts.disabled, minHovered, minPressed, barHeight);
        EmitWindowControlButton(ctx, maxBtnId, WinCtrlKind::Maximize, WindowHitRegion::MaximizeButton, opts.bindToWindow, maximized, focused, false, opts.disabled, maxHovered, maxPressed, barHeight);
        EmitWindowControlButton(ctx, clsBtnId, WinCtrlKind::Close, WindowHitRegion::CloseButton, opts.bindToWindow, maximized, focused, false, opts.disabled, clsHovered, clsPressed, barHeight);
    }
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
    const keel::f32 barH = opts.height.value_or(ResolveHeaderBarHeight(theme, opts.size));
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
                    // Only invoke StartSystemMove when click actually reached client area (X11/fallback)
                    host.startSystemMove();
                    ctx.ConsumeMouseClick();
                }
            }
        }
    }

    const keel::f32 barH = opts.height.value_or(ResolveHeaderBarHeight(theme, opts.size));
    const keel::f32 gap = opts.gap.value_or(6.0f);
    const Padding pad = opts.padding.value_or(Padding(8.0f, 0.0f, (placement == WindowControlsPlacement::Right ? 4.0f : 8.0f), 0.0f));

    node->direction = FlexDirection::Row;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Fixed;
    node->fixedHeight = barH;
    node->align = Alignment::Center;
    node->gap = gap;
    node->padding = pad;
    node->clipChildren = true;
    node->uiSize = opts.size;
    node->SetSurfaceRole(SurfaceRole::Flat);

    const Color defaultBg = focused ? theme.palette.surfaceElevated : theme.palette.surface;
    const Color resolvedBg = opts.background.IsBackgroundEnabled(false)
        ? theme.ResolveColor(opts.background, false, false, activeDisabled, defaultBg)
        : defaultBg;
    const Color resolvedBorder = (opts.border.width > 0.0f || opts.border.color.HasAny())
        ? theme.ResolveColor(opts.border.color, false, false, activeDisabled, theme.palette.borderSubtle)
        : Color::Transparent();
    node->backgroundColor = resolvedBg;
    node->borderColor = Color::Transparent();
    node->borderWidth = 0.0f;

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

    auto* renderPayload = ctx.AllocPayload<HeaderBarRenderPayload>(HeaderBarRenderPayload{
        .centeredTitle = allocTitle,
        .centeredSubtitle = allocSub,
        .centeredIcon = allocIcon,
        .centerTitle = opts.centerTitle,
        .focused = focused,
        .drawBottomBorder = (opts.border.width > 0.0f && resolvedBorder.a > 0u),
        .borderColor = resolvedBorder,
        .borderWidth = opts.border.width,
        .size = opts.size
    });
    node->SetPayload(renderPayload);
    node->renderFn = &RenderHeaderBarBackground;
    node->visualHash = detail::HashArgs(
        barH,
        resolvedBg.ToUint32(),
        resolvedBorder.ToUint32(),
        opts.border.width,
        opts.centerTitle,
        maximized,
        focused,
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
        titleNode->visualHash = detail::HashArgs(allocTitle.text, allocSub.text, focused, allocIcon.IsValid());
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
