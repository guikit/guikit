#include "guikit/core/Control.hpp"
#include <algorithm>
#include "guikit/backend/ICanvas.hpp"

namespace guikit {

float Control::ComputeWellPadding(
    const ControlSpec& spec,
    UiSize size,
    const UiTheme& theme
) noexcept {
    if (!HasWellFrame(spec)) {
        return 0.0f;
    }
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const float basePad = std::max(theme.Scale(4.0f), cm.paddingX * 0.5f);
    const float strokePad = (spec.borderWidth > 0.0f) ? theme.Scale(spec.borderWidth) : 0.0f;
    return basePad + strokePad;
}

Vec2 Control::Measure(
    Vec2 intrinsicSize,
    const ControlSpec& spec,
    StretchMode /*stretch*/,
    UiSize size,
    const UiTheme& theme
) noexcept {
    Vec2 result = intrinsicSize;
    const float wellPad = ComputeWellPadding(spec, size, theme);
    if (wellPad > 0.0f) {
        result.x += wellPad * 2.0f;
        result.y += wellPad * 2.0f;
    }
    if (spec.height.has_value() && *spec.height > 0.0f) {
        result.y = theme.Scale(*spec.height);
    }
    return result;
}

Rect Control::ResolveInnerRect(
    Rect controlRect,
    const ControlSpec& spec,
    UiSize size,
    const UiTheme& theme
) noexcept {
    const float wellPad = ComputeWellPadding(spec, size, theme);
    if (wellPad <= 0.0f) {
        return controlRect;
    }
    return controlRect.Inset(wellPad);
}

void Control::RenderFrame(
    ICanvas& canvas,
    Rect controlRect,
    const ControlSpec& spec,
    UiVariant variant,
    UiSize size,
    bool disabled,
    const UiTheme& theme
) noexcept {
    if (!HasWellFrame(spec) || controlRect.w <= 0.0f || controlRect.h <= 0.0f) {
        return;
    }

    const ComponentMetrics& cm = theme.GetMetrics(size);
    const float radius = spec.borderRadius.has_value()
        ? theme.Scale(*spec.borderRadius)
        : cm.cornerRadius;

    Color bg = Color::Transparent();
    if (spec.HasBackground(false)) {
        bg = theme.palette.surface;
        if (variant != UiVariant::Default && variant != UiVariant::Ghost && variant != UiVariant::Muted) {
            const Color accent = theme.GetVariant(variant).bg;
            bg = Color(
                static_cast<uint8_t>((static_cast<int>(bg.r) * 86 + static_cast<int>(accent.r) * 14) / 100),
                static_cast<uint8_t>((static_cast<int>(bg.g) * 86 + static_cast<int>(accent.g) * 14) / 100),
                static_cast<uint8_t>((static_cast<int>(bg.b) * 86 + static_cast<int>(accent.b) * 14) / 100),
                255
            );
        }
        if (disabled) {
            bg = bg.WithAlphaF(0.40f);
        }
    }

    const float strokeW = (spec.borderWidth > 0.0f) ? theme.Scale(spec.borderWidth) : 0.0f;
    Color borderCol = Color::Transparent();
    if (strokeW > 0.0f) {
        borderCol = (variant != UiVariant::Default && variant != UiVariant::Ghost && variant != UiVariant::Muted)
            ? theme.GetVariant(variant).bg.WithAlphaF(0.65f)
            : theme.palette.border;
        if (disabled) {
            borderCol = borderCol.WithAlphaF(0.40f);
        }
    }

    canvas.DrawRoundRect(controlRect, radius, bg, borderCol, strokeW);
}

} // namespace guikit
