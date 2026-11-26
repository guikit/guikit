#include "guikit/style/field_frame.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/style/style_painter.hpp"

namespace guikit {

Color FieldFrame::ResolveAccent(
    const UiTheme& theme,
    UiVariant status,
    bool disabled
) noexcept {
    const Color base = (status == UiVariant::Default || status == UiVariant::Primary)
        ? theme.palette.accent
        : theme.GetVariant(status).bg;
    return disabled ? base.WithAlphaF(theme.disabledOpacity) : base;
}

void FieldFrame::Draw(
    ICanvas& canvas,
    const UiTheme& theme,
    const FieldFrameParams& p
) {
    if (p.rect.w <= 0.0f || p.rect.h <= 0.0f) return;

    const auto& pal = theme.palette;
    const bool isActive = p.focused || p.active;
    const bool hasStatus = (p.status != UiVariant::Default);
    const Color statusCol = hasStatus ? theme.GetVariant(p.status).bg : pal.focusRing;

    const bool hasBg = p.background.IsBackgroundEnabled(p.style != FieldStyle::Borderless);
    Color baseFill = (p.defaultBgOverride.a > 0u) ? p.defaultBgOverride : pal.inputBackground;
    if (p.style == FieldStyle::Filled) {
        baseFill = (p.hovered && !p.disabled) ? pal.surfaceActive : pal.surfaceHover;
    } else if (p.style == FieldStyle::Borderless) {
        baseFill = (p.hovered && !p.disabled) ? pal.surfaceHover.WithAlphaF(0.45f) : Color::Transparent();
    } else if (p.hovered && !p.disabled && p.defaultBgOverride.a == 0u) {
        baseFill = pal.surface;
    }

    Color boxBg = hasBg ? baseFill : Color::Transparent();
    Color boxBorder = pal.inputBorder;
    keel::f32 strokeW = (p.border.width > 0.0f) ? p.border.width : p.defaultStrokeWidth;

    if (p.style == FieldStyle::Borderless && p.border.width <= 0.0f && !p.border.color.HasAny()) {
        if (isActive && !p.disabled) {
            boxBorder = hasStatus ? statusCol : pal.focusRing;
            strokeW = 1.0f;
        } else if (hasStatus && !p.disabled) {
            boxBorder = statusCol;
            strokeW = 1.0f;
        } else {
            boxBorder = Color::Transparent();
            strokeW = 0.0f;
        }
    } else if (p.style == FieldStyle::Filled && p.border.width <= 0.0f && !p.border.color.HasAny()) {
        if (isActive && !p.disabled) {
            boxBorder = hasStatus ? statusCol : pal.focusRing;
            strokeW = 1.5f;
        } else if (hasStatus && !p.disabled) {
            boxBorder = statusCol;
            strokeW = 1.0f;
        } else if (p.hovered && !p.disabled) {
            boxBorder = pal.borderHover;
            strokeW = 1.0f;
        } else {
            boxBorder = Color::Transparent();
            strokeW = 0.0f;
        }
    } else {
        if (isActive && !p.disabled) {
            boxBorder = hasStatus ? statusCol : pal.focusRing;
            if (strokeW > 0.0f) {
                strokeW = keel::max(strokeW + 0.5f, theme.focusRingWidth);
            }
        } else if (hasStatus && !p.disabled) {
            boxBorder = statusCol;
        } else if (p.hovered && !p.disabled) {
            boxBorder = pal.borderHover;
        }
    }

    if (p.disabled) {
        boxBg = boxBg.WithAlphaF(theme.disabledOpacity);
        boxBorder = boxBorder.WithAlphaF(theme.disabledOpacity);
    }

    if (hasBg && p.background.HasAny()) {
        if (!p.background.IsSurface()) {
            boxBg = theme.ResolveColor(p.background, p.hovered, isActive, p.disabled, boxBg);
        } else if (p.background.HasAlphaScale()) {
            boxBg = boxBg.ScaleAlpha(p.background.alphaScale);
        }
    }
    if (p.border.color.HasAny()) {
        boxBorder = theme.ResolveColor(p.border.color, p.hovered, isActive, p.disabled, boxBorder);
    }

    const keel::f32 radius = p.border.radius.has_value() ? *p.border.radius : p.defaultRadius;
    const bool isBorderlessNoBevel = (p.style == FieldStyle::Borderless) && !p.border.color.HasAny() && (p.border.width <= 0.0f);

    if (isBorderlessNoBevel) {
        if (boxBg.a > 0u || (strokeW > 0.0f && boxBorder.a > 0u)) {
            canvas.DrawRoundRect(p.rect, radius, boxBg, boxBorder, strokeW);
        }
        return;
    }

    theme.Painter().DrawFrame(canvas, theme, FrameKind::Input, FramePaintParams{
        .rect = p.rect,
        .radius = radius,
        .explicitRadius = p.border.radius.has_value(),
        .bg = boxBg,
        .border = boxBorder,
        .borderWidth = strokeW,
        .explicitBorder = p.border.HasExplicitStroke(),
        .hovered = p.hovered,
        .active = p.active,
        .focused = p.focused,
        .disabled = p.disabled,
        .accent = hasStatus ? statusCol : pal.focusRing,
        .hasStatusAccent = hasStatus && !p.disabled
    });
}

keel::f32 FieldFrame::DrawTrigger(
    ICanvas& canvas,
    const UiTheme& theme,
    Rect fieldRect,
    const Icon& icon,
    keel::f32 paddingX,
    keel::f32 iconSize,
    keel::f32 cornerRadius,
    bool explicitRadius,
    bool hovered,
    bool pressed,
    bool active,
    bool disabled,
    bool showHoverPill,
    Color fallbackIconCol
) {
    Rect iconRect{};
    Color iconCol = fallbackIconCol;
    const keel::f32 nextRightX = theme.Painter().DrawFieldButton(
        canvas, theme, fieldRect, paddingX, iconSize, cornerRadius, explicitRadius,
        hovered, pressed, active, disabled, showHoverPill, fallbackIconCol,
        iconRect, iconCol
    );
    if (icon.IsValid()) {
        icon.Draw(canvas, iconRect, iconCol);
    }
    return nextRightX;
}

} // namespace guikit
