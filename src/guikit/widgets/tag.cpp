#include "guikit/widgets/tag.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/style_painter.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

struct TagData {
    UiText text{};
    TagOptions opts{};
    bool hovered{false};
    bool pressed{false};
    bool closeHovered{false};
    bool closePressed{false};
};

struct TagMetrics {
    keel::f32 fontSize{12.0f};
    keel::f32 height{24.0f};
    keel::f32 padX{8.0f};
    keel::f32 gap{5.0f};
    keel::f32 iconSize{12.0f};
    keel::f32 closeSize{10.0f};
    keel::f32 closeHitW{16.0f};
    keel::f32 radius{5.0f};
};

TagMetrics ComputeTagMetrics(UiSize size, const UiTheme& theme) noexcept {
    TagMetrics m{};
    switch (size.GetPreset()) {
        case UiSize::Preset::Xs:
            m.fontSize = 11.0f;
            m.height = 20.0f;
            m.padX = 6.0f;
            m.gap = 4.0f;
            m.iconSize = 10.0f;
            m.closeSize = 9.0f;
            m.closeHitW = 14.0f;
            m.radius = 4.0f;
            break;
        case UiSize::Preset::Sm:
            m.fontSize = 12.0f;
            m.height = 24.0f;
            m.padX = 8.0f;
            m.gap = 5.0f;
            m.iconSize = 12.0f;
            m.closeSize = 10.0f;
            m.closeHitW = 16.0f;
            m.radius = 5.0f;
            break;
        case UiSize::Preset::Lg:
            m.fontSize = 14.0f;
            m.height = 32.0f;
            m.padX = 12.0f;
            m.gap = 6.0f;
            m.iconSize = 15.0f;
            m.closeSize = 12.0f;
            m.closeHitW = 20.0f;
            m.radius = 6.0f;
            break;
        case UiSize::Preset::Xl:
            m.fontSize = 15.0f;
            m.height = 36.0f;
            m.padX = 14.0f;
            m.gap = 7.0f;
            m.iconSize = 16.0f;
            m.closeSize = 13.0f;
            m.closeHitW = 22.0f;
            m.radius = 7.0f;
            break;
        case UiSize::Preset::Md:
            m.fontSize = 13.0f;
            m.height = 28.0f;
            m.padX = 10.0f;
            m.gap = 6.0f;
            m.iconSize = 14.0f;
            m.closeSize = 11.0f;
            m.closeHitW = 18.0f;
            m.radius = 6.0f;
            break;
        default: {
            const keel::f32 f = theme.SizeFactor(size);
            m.fontSize = keel::round(12.0f * f);
            m.height = keel::round(24.0f * f);
            m.padX = keel::round(8.0f * f);
            m.gap = keel::round(5.0f * f);
            m.iconSize = keel::round(12.0f * f);
            m.closeSize = keel::round(10.0f * f);
            m.closeHitW = keel::round(16.0f * f);
            m.radius = keel::round(5.0f * f);
            break;
        }
    }
    m.radius = theme.GetRadius(size);
    return m;
}

Rect ComputeCloseRect(const Rect& bounds, const TagMetrics& m) noexcept {
    const keel::f32 w = m.closeHitW;
    const keel::f32 h = keel::min(bounds.h, m.closeHitW + 2.0f);
    return Rect(
        bounds.Right() - m.padX * 0.65f - w,
        bounds.y + (bounds.h - h) * 0.5f,
        w,
        h
    );
}

Color ShadeLighten(Color c, keel::f32 amount) noexcept {
    return Color::Lerp(c, Color(255, 255, 255, c.a), amount);
}

Color ShadeDarken(Color c, keel::f32 amount) noexcept {
    return Color::Lerp(c, Color(0, 0, 0, c.a), amount);
}

struct TagPalette {
    Color bg{};
    Color border{};
    Color fg{};
};

TagPalette ResolveTagPalette(
    const TagOptions& opts,
    bool hovered,
    bool pressed,
    bool disabled,
    const UiTheme& theme
) noexcept {
    const auto& pal = theme.palette;
    const bool isDark = (theme.mode == ThemeMode::Dark);
    const bool isNeutral =
        !opts.color.HasAny() &&
        (opts.variant == UiVariant::Default || opts.variant == UiVariant::Secondary ||
         opts.variant == UiVariant::Muted || opts.variant == UiVariant::Ghost);

    TagPalette out{};

    if (isNeutral) {
        const Color baseBg = (opts.variant == UiVariant::Ghost)
            ? Color::Transparent()
            : (isDark ? pal.surfaceElevated : Color::Lerp(pal.surface, pal.surfaceHover, 0.75f));
        const Color baseBorder = (opts.variant == UiVariant::Ghost)
            ? Color::Transparent()
            : (isDark ? pal.borderHover : pal.border);
        const Color baseFg = (opts.variant == UiVariant::Muted) ? pal.textSecondary : pal.textPrimary;

        if (opts.style == TagStyle::Solid) {
            out.bg = isDark ? pal.surfaceActive : pal.textSecondary;
            out.border = out.bg;
            out.fg = isDark ? pal.textPrimary : Color(255, 255, 255, 255);
        } else if (opts.style == TagStyle::Outline) {
            out.bg = Color::Transparent();
            out.border = pal.borderHover;
            out.fg = baseFg;
        } else {
            out.bg = baseBg;
            out.border = baseBorder;
            out.fg = baseFg;
        }

        if (opts.clickable && !disabled) {
            if (pressed) {
                out.bg = (opts.style == TagStyle::Solid) ? ShadeDarken(out.bg, 0.12f) : pal.surfaceActive;
                out.border = pal.borderHover;
            } else if (hovered) {
                out.bg = (opts.style == TagStyle::Solid) ? ShadeLighten(out.bg, 0.08f) : pal.surfaceHover;
                out.border = pal.accent.WithAlphaF(0.55f);
            }
        }
    } else {
        const Color semHue = theme.GetSemanticAccent(opts.variant);
        const Color accent = opts.color.HasAny()
            ? theme.ResolveColor(opts.color, opts.clickable && hovered, opts.clickable && pressed, disabled, semHue)
            : semHue;

        if (opts.style == TagStyle::Solid) {
            out.bg = accent;
            out.border = Color::Transparent();
            out.fg = Color::BestTextColor(accent, Color::Hex(0x0F172AFF), Color::Hex(0xFFFFFFFF));
            if (opts.clickable && !disabled) {
                if (pressed) {
                    out.bg = ShadeDarken(accent, 0.12f);
                } else if (hovered) {
                    out.bg = isDark ? ShadeLighten(accent, 0.08f) : ShadeDarken(accent, 0.06f);
                }
            }
        } else if (opts.style == TagStyle::Outline) {
            out.bg = Color::Transparent();
            out.border = accent.WithAlphaF(isDark ? 0.65f : 0.55f);
            out.fg = isDark ? ShadeLighten(accent, 0.18f) : ShadeDarken(accent, 0.18f);
            if (opts.clickable && !disabled) {
                if (pressed) {
                    out.bg = accent.WithAlphaF(isDark ? 0.24f : 0.16f);
                } else if (hovered) {
                    out.bg = accent.WithAlphaF(isDark ? 0.14f : 0.09f);
                }
            }
        } else {
            // Soft
            const keel::f32 bgAlpha = isDark ? 0.18f : 0.12f;
            const keel::f32 borderAlpha = isDark ? 0.42f : 0.38f;
            out.bg = accent.WithAlphaF(bgAlpha);
            out.border = accent.WithAlphaF(borderAlpha);
            out.fg = isDark ? ShadeLighten(accent, 0.24f) : ShadeDarken(accent, 0.24f);
            if (opts.clickable && !disabled) {
                if (pressed) {
                    out.bg = accent.WithAlphaF(isDark ? 0.30f : 0.22f);
                    out.border = accent.WithAlphaF(0.65f);
                } else if (hovered) {
                    out.bg = accent.WithAlphaF(isDark ? 0.24f : 0.17f);
                    out.border = accent.WithAlphaF(0.55f);
                }
            }
        }
    }

    if (opts.border.color.HasAny()) {
        out.border = theme.ResolveColor(opts.border.color, opts.clickable && hovered, opts.clickable && pressed, disabled, out.border);
    }

    if (disabled) {
        out.bg = out.bg.WithAlphaF(keel::as<keel::f32>(out.bg.a) / 255.0f * theme.disabledOpacity);
        out.border = out.border.WithAlphaF(keel::as<keel::f32>(out.border.a) / 255.0f * theme.disabledOpacity);
        out.fg = out.fg.WithAlphaF(theme.disabledOpacity);
    }

    return out;
}

} // namespace

FontStyle ResolveTagFontStyle(const TagOptions& opts) noexcept {
    return (opts.variant != UiVariant::Default && opts.variant != UiVariant::Muted)
        ? FontStyle::Bold
        : FontStyle::Regular;
}

Vec2 Tag::Measure(
    const UiText& text,
    const TagOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const TagMetrics m = ComputeTagMetrics(size, theme);
    const keel::f32 fontSize = m.fontSize;
    const FontStyle style = ResolveTagFontStyle(opts);
    const keel::f32 textW = text.empty()
        ? 0.0f
        : (canvas != nullptr
            ? canvas->MeasureTextStyled(text.text, fontSize, style).x
            : TextUtil::MeasureUiTextWidth(text, fontSize, nullptr));

    keel::f32 w = m.padX * 2.0f + textW + 2.0f;
    if (opts.icon.IsValid()) {
        w += m.iconSize + (textW > 0.0f ? m.gap : 0.0f);
    }
    if (opts.closable) {
        w += m.closeHitW + (textW > 0.0f || opts.icon.IsValid() ? m.gap * 0.5f : 0.0f);
    }
    const keel::f32 h = keel::max(m.height, fontSize + 10.0f);
    return Vec2(keel::ceil(w), keel::ceil(h));
}

void Tag::Render(const Node* node, ICanvas& canvas, const UiTheme& theme) {
    const auto* data = node ? node->As<TagData>() : nullptr;
    if (!data) return;

    const Rect& rect = node->computedRect;
    if (rect.w <= 0.0f || rect.h <= 0.0f) return;

    const TagOptions& opts = data->opts;
    const TagMetrics m = ComputeTagMetrics(node->uiSize, theme);
    const keel::f32 fontSize = m.fontSize;
    const keel::f32 radius = opts.border.radius.has_value()
        ? *opts.border.radius
        : ((opts.pill && theme.controlStyle.pillBadgesAllowed) ? rect.h * 0.5f : m.radius);

    const TagPalette p = ResolveTagPalette(opts, data->hovered, data->pressed, node->disabled, theme);
    const keel::f32 borderW = (opts.style == TagStyle::Solid && !opts.border.color.HasAny())
        ? 0.0f
        : keel::max(0.0f, opts.border.width);

    theme.Painter().DrawFrame(canvas, theme, FrameKind::Badge, FramePaintParams{
        .rect = rect,
        .radius = keel::min(radius, rect.h * 0.5f),
        .explicitRadius = opts.border.radius.has_value(),
        .bg = p.bg,
        .border = borderW > 0.0f ? p.border : Color::Transparent(),
        .borderWidth = borderW,
        .hovered = data->hovered,
        .pressed = data->pressed,
        .disabled = node->disabled
    });

    keel::f32 curX = rect.x + m.padX;
    keel::f32 rightEdge = rect.Right() - m.padX;

    if (opts.icon.IsValid()) {
        const Rect icRect(curX, rect.y + (rect.h - m.iconSize) * 0.5f, m.iconSize, m.iconSize);
        opts.icon.Draw(canvas, icRect, p.fg);
        curX += m.iconSize + m.gap;
    }

    if (opts.closable) {
        const Rect closeRect = ComputeCloseRect(rect, m);
        if (data->closeHovered && !node->disabled) {
            const Color closeBg = data->closePressed
                ? p.fg.WithAlphaF(0.24f)
                : p.fg.WithAlphaF(0.14f);
            canvas.DrawRoundRect(closeRect, closeRect.h * 0.5f, closeBg);
        }
        const Rect xIconRect(
            closeRect.x + (closeRect.w - m.closeSize) * 0.5f,
            closeRect.y + (closeRect.h - m.closeSize) * 0.5f,
            m.closeSize,
            m.closeSize
        );
        Icon::Close().Draw(canvas, xIconRect, p.fg.WithAlphaF(data->closeHovered ? 1.0f : 0.78f));
        rightEdge = closeRect.x - m.gap * 0.5f;
    }

    if (!data->text.empty()) {
        const keel::f32 textW = keel::max(4.0f, rightEdge - curX);
        const Rect textRect(curX, rect.y, textW, rect.h);
        const FontStyle style = ResolveTagFontStyle(opts);
        canvas.DrawTextBoxStyled(data->text.text, textRect, fontSize, p.fg, style, 0.0f, 0.5f);
    }
}

TagResponse Tag::Run(
    Context& ctx,
    UiText text,
    const TagOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiTheme& theme = ctx.GetTheme();
    const TagMetrics m = ComputeTagMetrics(activeSize, theme);

    TagResponse res{};
    res.id = id;
    res.ctx = &ctx;

    const Rect prevRect = ctx.GetPrevFrameBounds(id);
    res.rect = prevRect;

    bool hovered = false;
    bool pressed = false;
    bool closeHovered = false;
    bool closePressed = false;

    if (!effectiveDisabled && prevRect.w > 0.0f && prevRect.h > 0.0f) {
        const Vec2 mouse = ctx.GetMousePos();
        const bool overTag = ctx.IsRectHovered(id, prevRect);
        if (overTag) {
            hovered = true;
            if (opts.closable) {
                const Rect closeRect = ComputeCloseRect(prevRect, m);
                closeHovered = closeRect.Contains(mouse);
            }

            if (closeHovered) {
                ctx.SetCursor(CursorType::Pointer);
                if (ctx.IsMouseDown()) {
                    closePressed = true;
                }
                if (ctx.IsMouseClicked()) {
                    res.closed = true;
                    res.changed = true;
                    ctx.ConsumeMouseClick();
                    ctx.InvalidateAll();
                }
            } else if (opts.clickable) {
                ctx.SetCursor(CursorType::Pointer);
                if (ctx.IsMouseDown()) {
                    pressed = true;
                }
                if (ctx.IsMouseClicked()) {
                    res.clicked = true;
                    res.changed = true;
                    ctx.ConsumeMouseClick();
                    ctx.InvalidateAll();
                }
            }

            if (!opts.tooltip.empty()) {
                ctx.SetTooltip(opts.tooltip, prevRect);
            }
        }
    }

    res.hovered = hovered;
    res.pressed = pressed;

    const ControlSpec spec = Control::FromOptions(
        opts,
        SizeMode::Fit,
        SizeMode::Fit,
        ElementLayout::Leading,
        4.0f,
        2.0f,
        8.0f
    );
    Node* node = ctx.CreateNode(NodeType::Tag, id);
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = hovered;
    node->pressed = pressed;

    auto* data = ctx.AllocPayload<TagData>();
    data->text = ctx.AllocText(text);
    data->opts = opts;
    data->opts.icon = ctx.AllocIcon(opts.icon);
    data->opts.tooltip = ctx.AllocText(opts.tooltip);
    data->hovered = hovered;
    data->pressed = pressed;
    data->closeHovered = closeHovered;
    data->closePressed = closePressed;

    node->text = data->text.text;
    node->SetPayload(data);
    node->measureFn = [](const Node* n, keel::f32 /*availW*/, ICanvas* canvas, const UiTheme& t) -> Vec2 {
        const auto* d = n->As<TagData>();
        if (!d) return Vec2(40.0f, 24.0f);
        return Tag::Measure(d->text, d->opts, n->uiSize, canvas, t);
    };
    node->renderFn = &Tag::Render;

    node->visualHash = detail::HashArgs(
        data->text,
        opts.icon,
        opts.variant,
        opts.style,
        activeSize,
        opts.pill,
        opts.clickable,
        opts.closable,
        effectiveDisabled,
        opts.color,
        opts.border,
        hovered,
        pressed,
        closeHovered,
        closePressed
    );
    Control::SetupNode(ctx, node, spec);
    return res;
}

} // namespace guikit
