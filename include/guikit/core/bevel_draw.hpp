#pragma once

#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/theme.hpp"
#include "keel/math.h"

namespace guikit::detail {

struct BevelColors {
    Color highlightOuter{};
    Color highlightInner{};
    Color shadowInner{};
    Color shadowOuter{};
};

[[nodiscard]] inline BevelColors ResolveBevelColors(const UiTheme& theme) noexcept {
    if (theme.mode == ThemeMode::Dark) {
        const Color base = theme.palette.surface.WithAlpha(255u);
        return BevelColors{
            .highlightOuter = base.Lighten(0.28f),
            .highlightInner = base.Lighten(0.13f),
            .shadowInner    = theme.palette.background.Darken(0.14f),
            .shadowOuter    = Color::Hex(0x000000E8)
        };
    }
    // Authentic Windows 98 DrawEdge(EDGE_RAISED / EDGE_SUNKEN) 4-tone bevel
    return BevelColors{
        .highlightOuter = Color::Hex(0xFFFFFFFF), // COLOR_3DHILIGHT
        .highlightInner = Color::Hex(0xDFDFDFFF), // COLOR_3DLIGHT
        .shadowInner    = Color::Hex(0x808080FF), // COLOR_3DSHADOW
        .shadowOuter    = Color::Hex(0x000000FF)  // COLOR_3DDKSHADOW
    };
}

/// Draws a pixel-crisp Windows 98 style raised or sunken 3D bevel frame.
/// - `thin == false`: classic 2px two-tone bevel (buttons, cards, menus, inputs, sliders)
/// - `thin == true`: subtle 1px bevel (flat toolbar hover/pressed, menu bar triggers, status tags)
inline void DrawBevelFrame(
    ICanvas& canvas,
    Rect r,
    const UiTheme& theme,
    bool sunken,
    bool thin = false
) {
    const keel::f32 x = keel::floor(r.x);
    const keel::f32 y = keel::floor(r.y);
    const keel::f32 w = keel::floor(r.w);
    const keel::f32 h = keel::floor(r.h);
    if (w < 3.0f || h < 3.0f) return;

    const BevelColors c = ResolveBevelColors(theme);
    const Color tlOuter = sunken ? c.shadowInner    : c.highlightOuter;
    const Color brOuter = sunken ? c.highlightOuter : c.shadowOuter;

    // Outer 1px rim
    canvas.DrawRect(Rect(x, y, w - 1.0f, 1.0f), tlOuter);               // Top
    canvas.DrawRect(Rect(x, y, 1.0f, h - 1.0f), tlOuter);               // Left
    canvas.DrawRect(Rect(x, y + h - 1.0f, w, 1.0f), brOuter);           // Bottom
    canvas.DrawRect(Rect(x + w - 1.0f, y, 1.0f, h), brOuter);           // Right

    if (thin || w < 5.0f || h < 5.0f) {
        return;
    }

    // Inner 1px rim
    const Color tlInner = sunken ? c.shadowOuter    : c.highlightInner;
    const Color brInner = sunken ? c.highlightInner : c.shadowInner;
    canvas.DrawRect(Rect(x + 1.0f, y + 1.0f, w - 3.0f, 1.0f), tlInner); // Inner top
    canvas.DrawRect(Rect(x + 1.0f, y + 1.0f, 1.0f, h - 3.0f), tlInner); // Inner left
    canvas.DrawRect(Rect(x + 1.0f, y + h - 2.0f, w - 2.0f, 1.0f), brInner); // Inner bottom
    canvas.DrawRect(Rect(x + w - 2.0f, y + 1.0f, 1.0f, h - 2.0f), brInner); // Inner right
}

/// Draws a classic Windows 98 2px etched groove frame (EDGE_ETCHED, as in GroupBox / Flat Card).
inline void DrawBevelEtchedFrame(
    ICanvas& canvas,
    Rect r,
    const UiTheme& theme
) {
    const keel::f32 x = keel::floor(r.x);
    const keel::f32 y = keel::floor(r.y);
    const keel::f32 w = keel::floor(r.w);
    const keel::f32 h = keel::floor(r.h);
    if (w < 4.0f || h < 4.0f) return;

    const BevelColors c = ResolveBevelColors(theme);
    // Highlight pass offset (+1, +1) drawn first, then shadow pass at (0, 0)
    canvas.DrawRect(Rect(x + 1.0f, y + 1.0f, w - 2.0f, 1.0f), c.highlightOuter); // Inner top
    canvas.DrawRect(Rect(x + 1.0f, y + 1.0f, 1.0f, h - 2.0f), c.highlightOuter); // Inner left
    canvas.DrawRect(Rect(x, y + h - 1.0f, w, 1.0f), c.highlightOuter);           // Outer bottom
    canvas.DrawRect(Rect(x + w - 1.0f, y, 1.0f, h), c.highlightOuter);           // Outer right

    canvas.DrawRect(Rect(x, y, w - 1.0f, 1.0f), c.shadowInner);                  // Outer top
    canvas.DrawRect(Rect(x, y, 1.0f, h - 1.0f), c.shadowInner);                  // Outer left
    canvas.DrawRect(Rect(x + 1.0f, y + h - 2.0f, w - 2.0f, 1.0f), c.shadowInner);// Inner bottom
    canvas.DrawRect(Rect(x + w - 2.0f, y + 1.0f, 1.0f, h - 2.0f), c.shadowInner);// Inner right
}

/// Draws a classic Windows 98 2px etched horizontal divider (shadow on top, highlight underneath).
inline void DrawBevelDividerH(
    ICanvas& canvas,
    keel::f32 x0,
    keel::f32 x1,
    keel::f32 y,
    const UiTheme& theme
) {
    const keel::f32 w = keel::floor(x1 - x0);
    if (w <= 0.0f) return;
    const BevelColors c = ResolveBevelColors(theme);
    const keel::f32 fx = keel::floor(x0);
    const keel::f32 fy = keel::floor(y);
    canvas.DrawRect(Rect(fx, fy, w, 1.0f), c.shadowInner);
    canvas.DrawRect(Rect(fx, fy + 1.0f, w, 1.0f), c.highlightOuter);
}

/// Draws a classic Windows 98 2px etched vertical divider (shadow on left, highlight on right).
inline void DrawBevelDividerV(
    ICanvas& canvas,
    keel::f32 x,
    keel::f32 y0,
    keel::f32 y1,
    const UiTheme& theme
) {
    const keel::f32 h = keel::floor(y1 - y0);
    if (h <= 0.0f) return;
    const BevelColors c = ResolveBevelColors(theme);
    const keel::f32 fx = keel::floor(x);
    const keel::f32 fy = keel::floor(y0);
    canvas.DrawRect(Rect(fx, fy, 1.0f, h), c.shadowInner);
    canvas.DrawRect(Rect(fx + 1.0f, fy, 1.0f, h), c.highlightOuter);
}

/// Draws a classic Windows 98 pentagonal pointed 3D slider thumb (TBS_BOTTOM / TBS_RIGHT).
inline void DrawBevelSliderThumb(
    ICanvas& canvas,
    Vec2 center,
    bool vertical,
    Color fill,
    const UiTheme& theme,
    bool focused = false
) {
    const BevelColors c = ResolveBevelColors(theme);
    if (!vertical) {
        // Horizontal slider: 11x20 pointed downward pentagon (Display Properties "Screen area")
        const keel::f32 w = 11.0f;
        const keel::f32 bodyH = 14.0f;
        const keel::f32 tipH = 5.0f;
        const keel::f32 totalH = bodyH + tipH;
        const keel::f32 x = keel::floor(center.x - w * 0.5f);
        const keel::f32 y = keel::floor(center.y - totalH * 0.5f);
        const keel::f32 cx = x + 5.0f;

        // Fill body + pointed bottom triangle
        canvas.DrawRect(Rect(x, y, w, bodyH), fill);
        for (keel::i32 row = 0; row < 5; ++row) {
            const keel::f32 inset = keel::as<keel::f32>(row + 1);
            const keel::f32 rw = w - inset * 2.0f;
            if (rw > 0.0f) {
                canvas.DrawRect(Rect(x + inset, y + bodyH + keel::as<keel::f32>(row), rw, 1.0f), fill);
            }
        }

        // Top & left raised bevel
        canvas.DrawRect(Rect(x, y, w - 1.0f, 1.0f), c.highlightOuter);
        canvas.DrawRect(Rect(x, y, 1.0f, bodyH), c.highlightOuter);
        canvas.DrawRect(Rect(x + 1.0f, y + 1.0f, w - 3.0f, 1.0f), c.highlightInner);
        canvas.DrawRect(Rect(x + 1.0f, y + 1.0f, 1.0f, bodyH - 1.0f), c.highlightInner);

        // Right raised bevel
        canvas.DrawRect(Rect(x + w - 1.0f, y, 1.0f, bodyH), c.shadowOuter);
        canvas.DrawRect(Rect(x + w - 2.0f, y + 1.0f, 1.0f, bodyH - 1.0f), c.shadowInner);

        // Stepped diagonal bevels along bottom point
        for (keel::i32 row = 0; row < 5; ++row) {
            const keel::f32 fy = y + bodyH + keel::as<keel::f32>(row);
            const keel::f32 lx = x + keel::as<keel::f32>(row + 1);
            const keel::f32 rx = x + w - 2.0f - keel::as<keel::f32>(row);
            canvas.DrawRect(Rect(lx, fy, 1.0f, 1.0f), c.highlightOuter);
            if (row < 4) {
                canvas.DrawRect(Rect(lx + 1.0f, fy, 1.0f, 1.0f), c.highlightInner);
                canvas.DrawRect(Rect(rx - 1.0f, fy, 1.0f, 1.0f), c.shadowInner);
            }
            canvas.DrawRect(Rect(rx, fy, 1.0f, 1.0f), c.shadowOuter);
        }
        canvas.DrawRect(Rect(cx, y + totalH - 1.0f, 1.0f, 1.0f), c.shadowOuter);

        if (focused) {
            canvas.DrawRoundRect(Rect(x + 2.0f, y + 2.0f, w - 4.0f, bodyH - 3.0f), 0.0f, Color::Transparent(), theme.palette.focusRing, 1.0f);
        }
    } else {
        // Vertical slider: 20x11 pointed rightward pentagon
        const keel::f32 h = 11.0f;
        const keel::f32 bodyW = 14.0f;
        const keel::f32 tipW = 5.0f;
        const keel::f32 totalW = bodyW + tipW;
        const keel::f32 x = keel::floor(center.x - totalW * 0.5f);
        const keel::f32 y = keel::floor(center.y - h * 0.5f);
        const keel::f32 cy = y + 5.0f;

        canvas.DrawRect(Rect(x, y, bodyW, h), fill);
        for (keel::i32 col = 0; col < 5; ++col) {
            const keel::f32 inset = keel::as<keel::f32>(col + 1);
            const keel::f32 rh = h - inset * 2.0f;
            if (rh > 0.0f) {
                canvas.DrawRect(Rect(x + bodyW + keel::as<keel::f32>(col), y + inset, 1.0f, rh), fill);
            }
        }

        canvas.DrawRect(Rect(x, y, bodyW, 1.0f), c.highlightOuter);
        canvas.DrawRect(Rect(x, y, 1.0f, h - 1.0f), c.highlightOuter);
        canvas.DrawRect(Rect(x + 1.0f, y + 1.0f, bodyW - 1.0f, 1.0f), c.highlightInner);
        canvas.DrawRect(Rect(x + 1.0f, y + 1.0f, 1.0f, h - 3.0f), c.highlightInner);
        canvas.DrawRect(Rect(x, y + h - 1.0f, bodyW, 1.0f), c.shadowOuter);
        canvas.DrawRect(Rect(x + 1.0f, y + h - 2.0f, bodyW - 1.0f, 1.0f), c.shadowInner);

        for (keel::i32 col = 0; col < 5; ++col) {
            const keel::f32 fx = x + bodyW + keel::as<keel::f32>(col);
            const keel::f32 ty = y + keel::as<keel::f32>(col + 1);
            const keel::f32 by = y + h - 2.0f - keel::as<keel::f32>(col);
            canvas.DrawRect(Rect(fx, ty, 1.0f, 1.0f), c.highlightOuter);
            if (col < 4) {
                canvas.DrawRect(Rect(fx, ty + 1.0f, 1.0f, 1.0f), c.highlightInner);
                canvas.DrawRect(Rect(fx, by - 1.0f, 1.0f, 1.0f), c.shadowInner);
            }
            canvas.DrawRect(Rect(fx, by, 1.0f, 1.0f), c.shadowOuter);
        }
        canvas.DrawRect(Rect(x + totalW - 1.0f, cy, 1.0f, 1.0f), c.shadowOuter);

        if (focused) {
            canvas.DrawRoundRect(Rect(x + 2.0f, y + 2.0f, bodyW - 3.0f, h - 4.0f), 0.0f, Color::Transparent(), theme.palette.focusRing, 1.0f);
        }
    }
}

/// Draws classic Windows 98 embossed disabled text (1px white drop + gray foreground on light bevel).
inline void DrawBevelDisabledTextBox(
    ICanvas& canvas,
    keel::StringRef text,
    Rect r,
    keel::f32 fontSize,
    const UiTheme& theme,
    keel::f32 alignX = 0.5f,
    keel::f32 alignY = 0.5f
) {
    if (theme.mode == ThemeMode::Light) {
        const Rect shadowR(r.x + 1.0f, r.y + 1.0f, r.w, r.h);
        canvas.DrawTextBox(text, shadowR, fontSize, Color::Hex(0xFFFFFFFF), alignX, alignY);
        canvas.DrawTextBox(text, r, fontSize, Color::Hex(0x808080FF), alignX, alignY);
    } else {
        canvas.DrawTextBox(text, r, fontSize, theme.palette.textDisabled, alignX, alignY);
    }
}

} // namespace guikit::detail
