#include "guikit/style/style_painter.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "keel/math.h"

namespace guikit {

// ==============================================================================
// Reusable 3D Bevel Primitives (Windows 98 EDGE_RAISED / EDGE_SUNKEN / EDGE_ETCHED)
// ==============================================================================

namespace {

[[nodiscard]] Rect SnapBevelRect(Rect r) noexcept {
    const keel::f32 x0 = keel::floor(r.x + 0.5f);
    const keel::f32 y0 = keel::floor(r.y + 0.5f);
    const keel::f32 x1 = keel::floor(r.Right() + 0.5f);
    const keel::f32 y1 = keel::floor(r.Bottom() + 0.5f);
    return Rect(x0, y0, keel::max(0.0f, x1 - x0), keel::max(0.0f, y1 - y0));
}

} // namespace

void DefaultStylePainter::DrawBevelFrame(
    ICanvas& canvas,
    Rect r,
    const BevelPalette& c,
    bool sunken,
    bool thin
) {
    const Rect snapped = SnapBevelRect(r);
    const keel::f32 x = snapped.x;
    const keel::f32 y = snapped.y;
    const keel::f32 w = snapped.w;
    const keel::f32 h = snapped.h;
    if (w < 3.0f || h < 3.0f) return;

    const Color tlOuter = sunken ? c.shadowInner    : c.highlightOuter;
    const Color brOuter = sunken ? c.highlightOuter : c.shadowOuter;

    canvas.DrawRect(Rect(x, y, w - 1.0f, 1.0f), tlOuter);
    canvas.DrawRect(Rect(x, y, 1.0f, h - 1.0f), tlOuter);
    canvas.DrawRect(Rect(x, y + h - 1.0f, w, 1.0f), brOuter);
    canvas.DrawRect(Rect(x + w - 1.0f, y, 1.0f, h), brOuter);

    if (thin || w < 5.0f || h < 5.0f) {
        return;
    }

    const Color tlInner = sunken ? c.shadowOuter    : c.highlightInner;
    const Color brInner = sunken ? c.highlightInner : c.shadowInner;
    canvas.DrawRect(Rect(x + 1.0f, y + 1.0f, w - 3.0f, 1.0f), tlInner);
    canvas.DrawRect(Rect(x + 1.0f, y + 1.0f, 1.0f, h - 3.0f), tlInner);
    canvas.DrawRect(Rect(x + 1.0f, y + h - 2.0f, w - 2.0f, 1.0f), brInner);
    canvas.DrawRect(Rect(x + w - 2.0f, y + 1.0f, 1.0f, h - 2.0f), brInner);
}

void DefaultStylePainter::DrawBevelEtchedFrame(
    ICanvas& canvas,
    Rect r,
    const BevelPalette& c
) {
    const Rect snapped = SnapBevelRect(r);
    const keel::f32 x = snapped.x;
    const keel::f32 y = snapped.y;
    const keel::f32 w = snapped.w;
    const keel::f32 h = snapped.h;
    if (w < 4.0f || h < 4.0f) return;

    canvas.DrawRect(Rect(x + 1.0f, y + 1.0f, w - 2.0f, 1.0f), c.highlightOuter);
    canvas.DrawRect(Rect(x + 1.0f, y + 1.0f, 1.0f, h - 2.0f), c.highlightOuter);
    canvas.DrawRect(Rect(x, y + h - 1.0f, w, 1.0f), c.highlightOuter);
    canvas.DrawRect(Rect(x + w - 1.0f, y, 1.0f, h), c.highlightOuter);

    canvas.DrawRect(Rect(x, y, w - 1.0f, 1.0f), c.shadowInner);
    canvas.DrawRect(Rect(x, y, 1.0f, h - 1.0f), c.shadowInner);
    canvas.DrawRect(Rect(x + 1.0f, y + h - 2.0f, w - 2.0f, 1.0f), c.shadowInner);
    canvas.DrawRect(Rect(x + w - 2.0f, y + 1.0f, 1.0f, h - 2.0f), c.shadowInner);
}

void DefaultStylePainter::DrawBevelDividerH(
    ICanvas& canvas,
    keel::f32 x0,
    keel::f32 x1,
    keel::f32 y,
    const BevelPalette& c
) {
    const keel::f32 w = keel::floor(x1 - x0);
    if (w <= 0.0f) return;
    const keel::f32 fx = keel::floor(x0);
    const keel::f32 fy = keel::floor(y);
    canvas.DrawRect(Rect(fx, fy, w, 1.0f), c.shadowInner);
    canvas.DrawRect(Rect(fx, fy + 1.0f, w, 1.0f), c.highlightOuter);
}

void DefaultStylePainter::DrawBevelDividerV(
    ICanvas& canvas,
    keel::f32 x,
    keel::f32 y0,
    keel::f32 y1,
    const BevelPalette& c
) {
    const keel::f32 h = keel::floor(y1 - y0);
    if (h <= 0.0f) return;
    const keel::f32 fx = keel::floor(x);
    const keel::f32 fy = keel::floor(y0);
    canvas.DrawRect(Rect(fx, fy, 1.0f, h), c.shadowInner);
    canvas.DrawRect(Rect(fx + 1.0f, fy, 1.0f, h), c.highlightOuter);
}

void DefaultStylePainter::DrawBevelSliderThumb(
    ICanvas& canvas,
    Vec2 center,
    keel::f32 thumbSize,
    bool vertical,
    Color fill,
    const BevelPalette& c,
    Color focusRing,
    bool focused
) {
    // Smooth vector 5-vertex pentagonal pointer scaled cleanly from thumbSize (0 staircase pixels at any DPI).
    const keel::f32 w = keel::round(keel::max(9.0f, thumbSize * 0.82f));
    const keel::f32 hw = w * 0.5f;
    const keel::f32 tipH = keel::round(keel::max(3.5f, w * 0.45f));
    const keel::f32 bodyH = keel::round(keel::max(9.0f, thumbSize * 1.04f));
    const keel::f32 totalH = bodyH + tipH;
    const keel::f32 halfTotal = totalH * 0.5f;
    const keel::f32 yTop = -halfTotal;
    const keel::f32 yShoulder = yTop + bodyH;
    const keel::f32 yTip = halfTotal;

    const keel::f32 diagLen = keel::sqrt(hw * hw + tipH * tipH);
    const keel::f32 tipInsetPerDp = diagLen / hw;
    const keel::f32 shoulderShiftPerDp = (diagLen - tipH) / hw;

    auto mapPt = [&](keel::f32 along, keel::f32 cross) noexcept -> Vec2 {
        return vertical
            ? Vec2(center.x + cross, center.y + along)
            : Vec2(center.x + along, center.y + cross);
    };

    auto insetVerts = [&](keel::f32 d, Vec2 (&out)[5]) noexcept {
        const keel::f32 ihw = keel::max(0.5f, hw - d);
        const keel::f32 iyTop = yTop + d;
        const keel::f32 iyShoulder = yShoulder - d * shoulderShiftPerDp;
        const keel::f32 iyTip = yTip - d * tipInsetPerDp;
        out[0] = mapPt(-ihw, iyTop);
        out[1] = mapPt(+ihw, iyTop);
        out[2] = mapPt(+ihw, iyShoulder);
        out[3] = mapPt(0.0f, iyTip);
        out[4] = mapPt(-ihw, iyShoulder);
    };

    // 1. Filled smooth vector pentagon
    Vec2 polyV[5];
    insetVerts(0.0f, polyV);
    VectorPath poly;
    poly.MoveTo(polyV[0]);
    for (keel::i32 i = 1; i < 5; ++i) {
        poly.LineTo(polyV[i]);
    }
    poly.Close();
    PathPaint fillPaint{};
    fillPaint.fillColor = fill;
    fillPaint.renderFill = true;
    fillPaint.renderStroke = false;
    canvas.DrawPath(poly, fillPaint);

    // 2. Smooth 2-layer 3D bevel along all 5 edges (lit: 0-1, 0-4, 4-3; shadow: 1-2, 2-3)
    const keel::f32 strokeW = 1.0f;
    if (hw >= 3.0f) {
        Vec2 inV[5];
        insetVerts(1.5f, inV);
        canvas.DrawLine(inV[0], inV[1], c.highlightInner, strokeW);
        canvas.DrawLine(inV[0], inV[4], c.highlightInner, strokeW);
        canvas.DrawLine(inV[4], inV[3], c.highlightInner, strokeW);
        canvas.DrawLine(inV[1], inV[2], c.shadowInner, strokeW);
        canvas.DrawLine(inV[2], inV[3], c.shadowInner, strokeW);
    }

    Vec2 outV[5];
    insetVerts(0.5f, outV);
    canvas.DrawLine(outV[0], outV[1], c.highlightOuter, strokeW);
    canvas.DrawLine(outV[0], outV[4], c.highlightOuter, strokeW);
    canvas.DrawLine(outV[4], outV[3], c.highlightOuter, strokeW);
    canvas.DrawLine(outV[1], outV[2], c.shadowOuter, strokeW);
    canvas.DrawLine(outV[2], outV[3], c.shadowOuter, strokeW);

    if (focused && hw >= 3.5f && bodyH >= 6.0f) {
        const Rect fRect = vertical
            ? Rect(center.x + yTop + 2.5f, center.y - hw + 2.5f, bodyH - 3.5f, w - 5.0f)
            : Rect(center.x - hw + 2.5f, center.y + yTop + 2.5f, w - 5.0f, bodyH - 3.5f);
        canvas.DrawRoundRect(fRect, 0.0f, Color::Transparent(), focusRing, 1.0f);
    }
}

// ==============================================================================
// DefaultStylePainter — Unified Surface & Control Rendering
// ==============================================================================

namespace {

void DrawShadowForRect(
    ICanvas& canvas,
    const UiTheme& theme,
    Rect r,
    keel::f32 radius,
    Elevation elev,
    keel::f32 shadowScale
) {
    if (elev == Elevation::None) return;
    const ShadowStyle sh = theme.GetShadow(elev, shadowScale);
    if (!sh.IsVisible()) return;
    if (sh.blur <= 0.01f) {
        const Rect hardR(r.x + sh.offset.x, r.y + sh.offset.y, r.w, r.h);
        canvas.DrawRect(hardR, sh.color);
    } else {
        canvas.DrawShadow(r, radius, sh.blur, sh.offset, sh.color);
    }
}

/// Subtle 1px top-bright / bottom-dark directional lighting for `FrameTreatment::Directional1px` (`Fluent`).
void DrawDirectionalSheen(
    ICanvas& canvas,
    const UiTheme& theme,
    Rect r,
    keel::f32 radius,
    bool pressed,
    bool includeBottomShade
) {
    if (r.w < 6.0f || r.h < 6.0f) return;
    const keel::f32 inset = keel::clamp(radius * 0.65f, 1.0f, r.w * 0.25f);
    const keel::f32 x0 = r.x + inset;
    const keel::f32 x1 = r.Right() - inset;
    if (x1 <= x0) return;

    const bool isDark = (theme.mode == ThemeMode::Dark);
    if (!pressed) {
        const Color hi = theme.bevel.highlightOuter.WithAlphaF(isDark ? 0.14f : 0.45f);
        canvas.DrawLine(Vec2(x0, r.y + 0.75f), Vec2(x1, r.y + 0.75f), hi, 1.0f);
    }
    if (includeBottomShade) {
        const Color lo = theme.bevel.shadowOuter.WithAlphaF(isDark ? 0.34f : 0.14f);
        canvas.DrawLine(Vec2(x0, r.Bottom() - 0.75f), Vec2(x1, r.Bottom() - 0.75f), lo, 1.0f);
    }
}

} // namespace

void DefaultStylePainter::DrawSurfaceBg(
    ICanvas& canvas,
    const UiTheme& theme,
    const SurfacePaintParams& p
) const {
    if (p.rect.w <= 0.0f || p.rect.h <= 0.0f) return;

    const bool roundedOverride = p.explicitRadius && (p.radius > 0.01f);
    const bool useBevel = (theme.chrome.frame == FrameTreatment::Bevel3D) && !roundedOverride;
    const keel::f32 effRadius = useBevel ? 0.0f : p.radius;

    Elevation effElev = p.elevation;
    if (effElev == Elevation::None) {
        if (p.role == SurfaceRole::Raised) effElev = Elevation::Md;
        else if (p.role == SurfaceRole::Popup) effElev = Elevation::Lg;
    }
    DrawShadowForRect(canvas, theme, p.rect, effRadius, effElev, p.shadowScale);

    if (p.bg.a > 0u) {
        if (effRadius > 0.0f) {
            canvas.DrawRoundRect(p.rect, effRadius, p.bg, Color::Transparent(), 0.0f);
        } else {
            canvas.DrawRect(useBevel ? SnapBevelRect(p.rect) : p.rect, p.bg);
        }
    }
}

void DefaultStylePainter::DrawSurfaceFrame(
    ICanvas& canvas,
    const UiTheme& theme,
    const SurfacePaintParams& p
) const {
    if (p.rect.w <= 0.0f || p.rect.h <= 0.0f || p.role == SurfaceRole::None) return;

    const Rect r = p.rect;
    const bool roundedOverride = p.explicitRadius && (p.radius > 0.01f);
    const bool useBevel = (theme.chrome.frame == FrameTreatment::Bevel3D) && !roundedOverride;

    // Docked single-edge seam for shell panels (Sidebar, Toolbar, MenuBar, BottomPanel, StatusBar)
    if (p.role == SurfaceRole::DockEdge && p.dockSeam != DockSeam::None && p.radius <= 0.01f) {
        if (p.borderWidth <= 0.0f || p.border.a == 0u) return;
        const keel::f32 seamThickness = (theme.chrome.divider == DividerStyle::Etched3D) ? 2.0f : p.borderWidth;
        switch (p.dockSeam) {
            case DockSeam::Left:
                DrawDivider(canvas, theme, Vec2(r.x, r.y), Vec2(r.x, r.Bottom()), p.border, p.borderWidth);
                return;
            case DockSeam::Right: {
                const keel::f32 x = keel::max(r.x, r.Right() - seamThickness);
                DrawDivider(canvas, theme, Vec2(x, r.y), Vec2(x, r.Bottom()), p.border, p.borderWidth);
                return;
            }
            case DockSeam::Top:
                DrawDivider(canvas, theme, Vec2(r.x, r.y), Vec2(r.Right(), r.y), p.border, p.borderWidth);
                return;
            case DockSeam::Bottom: {
                const keel::f32 y = keel::max(r.y, r.Bottom() - seamThickness);
                DrawDivider(canvas, theme, Vec2(r.x, y), Vec2(r.Right(), y), p.border, p.borderWidth);
                return;
            }
            case DockSeam::None:
                break;
        }
    }

    if (useBevel) {
        if (p.borderWidth <= 0.0f && p.role != SurfaceRole::Popup && p.role != SurfaceRole::Raised && p.role != SurfaceRole::Sunken) {
            return;
        }
        switch (p.role) {
            case SurfaceRole::Flat:
                DrawBevelEtchedFrame(canvas, r, theme.bevel);
                return;
            case SurfaceRole::Sunken:
                DrawBevelFrame(canvas, r, theme.bevel, /*sunken=*/true, /*thin=*/false);
                return;
            case SurfaceRole::Raised:
            case SurfaceRole::Popup:
            case SurfaceRole::DockEdge:
                DrawBevelFrame(canvas, r, theme.bevel, /*sunken=*/false, /*thin=*/false);
                return;
            case SurfaceRole::None:
                return;
        }
    }

    if (p.borderWidth > 0.0f && p.border.a > 0u) {
        const keel::f32 bw = (theme.chrome.frame == FrameTreatment::HardShadow)
            ? keel::max(p.borderWidth, 1.5f * keel::max(theme.borderScale, 0.5f))
            : p.borderWidth;
        canvas.DrawRoundRect(r, p.radius, Color::Transparent(), p.border, bw);
    }

    if (theme.chrome.frame == FrameTreatment::Directional1px &&
        (p.role == SurfaceRole::Raised || p.role == SurfaceRole::Popup || p.role == SurfaceRole::Flat)) {
        DrawDirectionalSheen(canvas, theme, r, p.radius, /*pressed=*/false, /*includeBottomShade=*/(p.role == SurfaceRole::Raised));
    }
}

void DefaultStylePainter::DrawFrame(
    ICanvas& canvas,
    const UiTheme& theme,
    FrameKind kind,
    const FramePaintParams& p
) const {
    if (p.rect.w <= 0.0f || p.rect.h <= 0.0f) return;

    const bool isInput = (kind == FrameKind::Input);
    const bool roundedOverride = p.explicitRadius && (p.radius > 0.01f);
    const bool useBevel = isInput
        ? ((theme.chrome.field == FieldTreatment::Sunken3D) && !roundedOverride)
        : ((theme.chrome.frame == FrameTreatment::Bevel3D) && !roundedOverride);
    const keel::f32 effRadius = useBevel ? 0.0f : p.radius;

    Elevation effElev = p.elevation;
    if (effElev == Elevation::None && kind == FrameKind::Button &&
        theme.chrome.frame == FrameTreatment::HardShadow &&
        !p.disabled && !p.pressed && !p.active && p.bg.a > 0u) {
        effElev = Elevation::Sm;
    }
    DrawShadowForRect(canvas, theme, p.rect, effRadius, effElev, p.shadowScale);

    // Non-Bevel path: Flat (Modern), Directional1px (Fluent), HardShadow (Brutalist)
    if (!useBevel) {
        if (isInput && theme.chrome.field == FieldTreatment::BottomAccent) {
            const Color baseBorder = p.disabled
                ? p.border
                : theme.palette.border;
            const keel::f32 baseBw = (p.borderWidth > 0.0f) ? keel::min(p.borderWidth, 1.0f) : 1.0f;
            canvas.DrawRoundRect(p.rect, effRadius, p.bg, baseBorder, baseBw);

            if (p.rect.w > 6.0f && p.rect.h > 6.0f) {
                const keel::f32 inset = keel::clamp(effRadius * 0.55f, 1.0f, p.rect.w * 0.25f);
                const keel::f32 x0 = p.rect.x + inset;
                const keel::f32 x1 = p.rect.Right() - inset;
                if (x1 > x0) {
                    const Color accentBar = p.focused && !p.disabled
                        ? ((p.accent.a > 0u) ? p.accent : theme.palette.focusRing)
                        : (p.disabled ? theme.palette.border.WithAlphaF(theme.disabledOpacity) : theme.palette.borderHover);
                    const keel::f32 barH = (p.focused && !p.disabled) ? 2.0f : 1.0f;
                    const keel::f32 barY = p.rect.Bottom() - barH * 0.5f;
                    canvas.DrawLine(Vec2(x0, barY), Vec2(x1, barY), accentBar, barH);
                }
            }
            return;
        }

        if (effRadius > 0.0f) {
            canvas.DrawRoundRect(p.rect, effRadius, p.bg, p.border, p.borderWidth);
        } else if (p.borderWidth > 0.0f && p.border.a > 0u) {
            canvas.DrawRoundRect(p.rect, 0.0f, p.bg, p.border, p.borderWidth);
        } else if (p.bg.a > 0u) {
            canvas.DrawRect(p.rect, p.bg);
        }

        if (theme.chrome.frame == FrameTreatment::Directional1px && !p.disabled && p.bg.a > 0u &&
            (kind == FrameKind::Button || kind == FrameKind::DropdownButton)) {
            DrawDirectionalSheen(canvas, theme, p.rect, effRadius, p.pressed || p.active, /*includeBottomShade=*/true);
        }
        return;
    }

    // FrameTreatment::Bevel3D / FieldTreatment::Sunken3D path
    const Rect bevelRect = SnapBevelRect(p.rect);
    const Color effBg = (isInput && p.disabled && p.bg.a > 0u) ? theme.palette.surface : p.bg;
    if (effBg.a > 0u) {
        canvas.DrawRect(bevelRect, effBg);
    }

    switch (kind) {
        case FrameKind::Input:
            DrawBevelFrame(canvas, bevelRect, theme.bevel, /*sunken=*/true, /*thin=*/false);
            if (p.focused && !p.disabled && bevelRect.w > 6.0f && bevelRect.h > 6.0f) {
                canvas.DrawRoundRect(
                    Rect(bevelRect.x + 2.0f, bevelRect.y + 2.0f, bevelRect.w - 4.0f, bevelRect.h - 4.0f),
                    0.0f, Color::Transparent(), (p.accent.a > 0u) ? p.accent : theme.palette.focusRing, 1.0f
                );
            }
            break;

        case FrameKind::Button:
        case FrameKind::DropdownButton:
            if (effBg.a > 0u) {
                DrawBevelFrame(canvas, bevelRect, theme.bevel, /*sunken=*/(!p.disabled && (p.pressed || p.active)), /*thin=*/false);
            }
            if (p.focused && !p.disabled && bevelRect.w > 8.0f && bevelRect.h > 8.0f) {
                canvas.DrawRoundRect(
                    Rect(bevelRect.x + 3.0f, bevelRect.y + 3.0f, bevelRect.w - 6.0f, bevelRect.h - 6.0f),
                    0.0f, Color::Transparent(), theme.palette.focusRing, 1.0f
                );
            }
            break;

        case FrameKind::ToolbarButton:
            if (!p.disabled && (p.pressed || p.active)) {
                DrawBevelFrame(canvas, bevelRect, theme.bevel, /*sunken=*/true, /*thin=*/true);
            } else if (!p.disabled && p.hovered) {
                DrawBevelFrame(canvas, bevelRect, theme.bevel, /*sunken=*/false, /*thin=*/true);
            }
            if (p.focused && !p.disabled && bevelRect.w > 6.0f && bevelRect.h > 6.0f) {
                canvas.DrawRoundRect(
                    Rect(bevelRect.x + 2.0f, bevelRect.y + 2.0f, bevelRect.w - 4.0f, bevelRect.h - 4.0f),
                    0.0f, Color::Transparent(), theme.palette.focusRing, 1.0f
                );
            }
            break;

        case FrameKind::Badge:
            DrawBevelFrame(canvas, bevelRect, theme.bevel, /*sunken=*/true, /*thin=*/true);
            break;
    }
}

ChromeMetrics DefaultStylePainter::GetChromeMetrics(const UiTheme& theme) const noexcept {
    ChromeMetrics m{};
    m.buttonPressOffset = theme.chrome.buttonPressOffset;
    m.tabGapOverride = theme.chrome.tabGapOverride;
    if (theme.chrome.frame == FrameTreatment::Bevel3D) {
        m.frameBevelInset = 2.0f;
        m.sliderTrackHeight = 4.0f;
        m.rangeSliderTrackHeight = 6.0f;
    }
    if (theme.chrome.progressFill == ProgressFillStyle::Segmented) {
        m.progressBarMinHeight = 14.0f;
        m.progressBarHeightBonus = 4.0f;
    }
    m.badgeShape = theme.chrome.badgeShape;
    m.solidSelectionHighlight = (theme.chrome.selection == SelectionHighlightStyle::SolidAccent);
    return m;
}

keel::f32 DefaultStylePainter::DrawFieldButton(
    ICanvas& canvas,
    const UiTheme& theme,
    Rect fieldRect,
    keel::f32 paddingX,
    keel::f32 iconSize,
    keel::f32 cornerRadius,
    bool explicitRadius,
    bool hovered,
    bool pressed,
    bool active,
    bool disabled,
    bool showHoverPill,
    Color fallbackIconCol,
    Rect& outIconRect,
    Color& outIconColor
) const {
    const auto& pal = theme.palette;
    const bool roundedOverride = explicitRadius && (cornerRadius > 0.01f);
    const bool useRaised = (theme.chrome.fieldButton == FieldButtonStyle::Raised3D) && !roundedOverride;
    const bool useFramed = (theme.chrome.fieldButton == FieldButtonStyle::Framed);

    if (useRaised || useFramed) {
        const keel::f32 inset = useRaised ? 2.0f : 1.0f;
        const keel::f32 dropBtnW = keel::max(18.0f, keel::min(fieldRect.h - inset * 2.0f, paddingX + iconSize + 4.0f));
        const Rect dropBtnRect(
            keel::floor(fieldRect.Right() - inset - dropBtnW),
            keel::floor(fieldRect.y + inset),
            keel::floor(dropBtnW),
            keel::max(4.0f, keel::floor(fieldRect.h - inset * 2.0f))
        );
        const Color btnBg = disabled
            ? pal.surface.WithAlphaF(theme.disabledOpacity)
            : ((pressed || active) ? pal.surfaceActive : (hovered ? pal.surfaceHover : pal.surface));
        DrawFrame(canvas, theme, FrameKind::DropdownButton, FramePaintParams{
            .rect = dropBtnRect,
            .radius = useRaised ? 0.0f : keel::max(0.0f, cornerRadius - 1.0f),
            .explicitRadius = explicitRadius,
            .bg = btnBg,
            .border = useFramed ? pal.border : Color::Transparent(),
            .borderWidth = useFramed ? 1.0f : 0.0f,
            .hovered = hovered,
            .pressed = pressed,
            .active = active,
            .disabled = disabled,
            .accent = pal.accent
        });
        const keel::f32 shift = GetButtonContentOffset(theme, pressed, active, disabled);
        outIconRect = Rect(
            dropBtnRect.x + (dropBtnRect.w - iconSize) * 0.5f + shift,
            dropBtnRect.y + (dropBtnRect.h - iconSize) * 0.5f + shift,
            iconSize,
            iconSize
        );
        outIconColor = disabled ? pal.textDisabled : pal.textPrimary;
        return dropBtnRect.x - 4.0f;
    }

    const keel::f32 iconX = fieldRect.x + fieldRect.w - paddingX - iconSize;
    if (showHoverPill && hovered && !disabled) {
        const keel::f32 btnSize = iconSize + 4.0f;
        const Rect chevBtnRect(iconX - 2.0f, fieldRect.y + (fieldRect.h - btnSize) * 0.5f, btnSize, btnSize);
        canvas.DrawRoundRect(chevBtnRect, keel::min(4.0f, cornerRadius), pal.surfaceHover, Color::Transparent(), 0.0f);
    }
    outIconRect = Rect(iconX, fieldRect.y + (fieldRect.h - iconSize) * 0.5f, iconSize, iconSize);
    outIconColor = fallbackIconCol;
    return iconX - 6.0f;
}

void DefaultStylePainter::DrawDropdownSurface(
    ICanvas& canvas,
    const UiTheme& theme,
    Rect popupRect,
    keel::f32 cornerRadius
) const {
    if (popupRect.w <= 0.0f || popupRect.h <= 0.0f) return;
    const auto& pal = theme.palette;
    if (theme.chrome.frame == FrameTreatment::Bevel3D) {
        DrawShadowForRect(canvas, theme, popupRect, 0.0f, Elevation::Md, 1.0f);
        canvas.DrawRoundRect(popupRect, 0.0f, pal.inputBackground, theme.bevel.shadowOuter, 1.0f);
        return;
    }
    DrawSurface(canvas, theme, SurfacePaintParams{
        .rect = popupRect,
        .role = SurfaceRole::Popup,
        .radius = cornerRadius,
        .bg = pal.surfaceElevated,
        .border = pal.border,
        .borderWidth = 1.0f,
        .elevation = Elevation::Md
    });
}

void DefaultStylePainter::DrawSliderTick(
    ICanvas& canvas,
    const UiTheme& theme,
    Vec2 tickCenter,
    keel::f32 crossCoord,
    bool vertical,
    keel::f32 tickSize,
    keel::f32 thumbSize,
    Color color,
    bool disabled
) const {
    if (theme.chrome.sliderThumb == SliderThumbShape::Pointer3D) {
        const Color tickCol = disabled ? theme.palette.textDisabled : theme.palette.border;
        const keel::f32 tickOffset = keel::round(keel::max(6.0f, thumbSize * 0.75f));
        if (vertical) {
            canvas.DrawRect(Rect(keel::floor(crossCoord + tickOffset), keel::floor(tickCenter.y), 3.0f, 1.0f), tickCol);
        } else {
            canvas.DrawRect(Rect(keel::floor(tickCenter.x), keel::floor(crossCoord + tickOffset), 1.0f, 3.0f), tickCol);
        }
        return;
    }
    const keel::f32 tickR = tickSize * 0.5f;
    canvas.DrawRoundRect(
        Rect(tickCenter.x - tickR, tickCenter.y - tickR, tickSize, tickSize),
        tickR,
        color
    );
}

void DefaultStylePainter::DrawDivider(
    ICanvas& canvas,
    const UiTheme& theme,
    Vec2 p0,
    Vec2 p1,
    Color fallbackColor,
    keel::f32 thickness
) const {
    const bool isHoriz = keel::abs(p1.x - p0.x) >= keel::abs(p1.y - p0.y);
    if (theme.chrome.divider == DividerStyle::Etched3D) {
        if (isHoriz) {
            DrawBevelDividerH(canvas, keel::min(p0.x, p1.x), keel::max(p0.x, p1.x), keel::floor(p0.y), theme.bevel);
        } else {
            DrawBevelDividerV(canvas, keel::floor(p0.x), keel::min(p0.y, p1.y), keel::max(p0.y, p1.y), theme.bevel);
        }
        return;
    }
    if (fallbackColor.a == 0u || thickness <= 0.0f) return;
    const keel::f32 effThick = (theme.chrome.divider == DividerStyle::Bold) ? keel::max(thickness, 2.0f) : thickness;
    canvas.DrawLine(p0, p1, fallbackColor, effThick);
}

void DefaultStylePainter::DrawControlText(
    ICanvas& canvas,
    const UiTheme& theme,
    keel::StringRef text,
    Rect rect,
    keel::f32 fontSize,
    Color color,
    bool disabled,
    keel::f32 alignX,
    keel::f32 alignY
) const {
    if (disabled && theme.chrome.disabledText == DisabledTextStyle::Embossed3D) {
        const Rect shadowR(rect.x + 1.0f, rect.y + 1.0f, rect.w, rect.h);
        const Color hiCol = (theme.mode == ThemeMode::Dark)
            ? theme.bevel.highlightOuter.WithAlphaF(0.32f)
            : theme.bevel.highlightOuter;
        const Color fgCol = (theme.mode == ThemeMode::Dark)
            ? theme.palette.textDisabled
            : theme.bevel.shadowInner;
        canvas.DrawTextBox(text, shadowR, fontSize, hiCol, alignX, alignY);
        canvas.DrawTextBox(text, rect, fontSize, fgCol, alignX, alignY);
        return;
    }
    canvas.DrawTextBox(text, rect, fontSize, color, alignX, alignY);
}

void DefaultStylePainter::DrawSliderTrack(
    ICanvas& canvas,
    const UiTheme& theme,
    Rect trackRect,
    Rect fillRect,
    bool vertical,
    bool hasFill,
    Color trackBg,
    Color trackBorder,
    Color fillColor,
    keel::f32 cornerRadius,
    bool explicitColor
) const {
    const auto& pal = theme.palette;
    const bool isBevel = (theme.chrome.frame == FrameTreatment::Bevel3D);
    const bool isBrutal = (theme.chrome.frame == FrameTreatment::HardShadow);
    const bool sharp = isBevel || isBrutal || (cornerRadius <= 0.01f);
    const keel::f32 trackR = sharp ? 0.0f : ((vertical ? trackRect.w : trackRect.h) * 0.5f);

    if (isBevel) {
        const Color grooveBg = (Color::ContrastRatio(pal.background, pal.surface) >= 1.08f)
            ? pal.background
            : pal.controlTrack;
        canvas.DrawRect(trackRect, grooveBg);
        DrawBevelFrame(canvas, trackRect, theme.bevel, /*sunken=*/true, /*thin=*/false);
    } else if (isBrutal) {
        const keel::f32 bw = keel::max(1.5f, 1.5f * keel::max(theme.borderScale, 0.5f));
        const Color bdr = (Color::ContrastRatio(trackBorder, trackBg) >= 1.25f)
            ? trackBorder
            : pal.borderHover;
        canvas.DrawRoundRect(trackRect, 0.0f, trackBg, bdr, bw);
    } else {
        canvas.DrawRoundRect(trackRect, trackR, trackBg, trackBorder, 1.0f);
    }

    if (hasFill && (theme.chrome.sliderTrack == SliderTrackStyle::Filled || explicitColor)) {
        if (sharp) {
            if (isBevel) {
                const Rect insetFill(
                    fillRect.x + (vertical ? 1.5f : 1.0f),
                    fillRect.y + (vertical ? 0.0f : 1.5f),
                    keel::max(1.0f, fillRect.w - (vertical ? 3.0f : 1.0f)),
                    keel::max(1.0f, fillRect.h - (vertical ? 1.0f : 3.0f))
                );
                canvas.DrawRect(insetFill, fillColor);
            } else {
                canvas.DrawRect(fillRect, fillColor);
            }
        } else {
            canvas.DrawRoundRect(fillRect, trackR, fillColor);
        }
    }
}

void DefaultStylePainter::DrawSliderThumb(
    ICanvas& canvas,
    const UiTheme& theme,
    const SliderThumbPaintParams& p
) const {
    const auto& pal = theme.palette;
    const Color accent = (p.accent.a > 0u) ? p.accent : pal.accent;
    const SliderThumbShape shape = theme.chrome.sliderThumb;

    // 1. Classic 3D vector pentagonal pointer
    if (shape == SliderThumbShape::Pointer3D) {
        const Color thumbBg = p.disabled ? pal.surface.WithAlphaF(theme.disabledOpacity) :
                              p.active   ? pal.surfaceActive.WithAlpha(255u) :
                              p.hovered  ? pal.surfaceHover.WithAlpha(255u) : pal.surface;
        DrawBevelSliderThumb(canvas, p.center, p.thumbSize, p.vertical, thumbBg, theme.bevel, pal.focusRing, p.focused && !p.disabled);
        return;
    }

    // 2. Fluent concentric ring (outer elevated shell + responsive inner accent core)
    if (shape == SliderThumbShape::Ring) {
        const keel::f32 halfThumb = p.thumbSize * 0.5f;
        const Rect outerRect(p.center.x - halfThumb, p.center.y - halfThumb, p.thumbSize, p.thumbSize);
        if (!p.disabled && theme.shadowsEnabled) {
            DrawShadowForRect(canvas, theme, outerRect, halfThumb, Elevation::Sm, 0.55f);
        }
        const Color outerBg = p.disabled ? pal.surface.WithAlphaF(theme.disabledOpacity) : pal.surfaceElevated;
        const Color outerBdr = p.disabled ? pal.border.WithAlphaF(theme.disabledOpacity) :
                               p.focused  ? pal.focusRing :
                               (p.hovered || p.active) ? pal.borderHover : pal.border;
        const keel::f32 outerBw = p.focused ? 2.0f : 1.0f;
        canvas.DrawRoundRect(outerRect, halfThumb, outerBg, outerBdr, outerBw);

        const keel::f32 coreRatio = p.active ? 0.42f : (p.hovered ? 0.64f : 0.52f);
        const keel::f32 coreDiam = keel::max(4.0f, p.thumbSize * coreRatio);
        const keel::f32 coreHalf = coreDiam * 0.5f;
        const Color coreCol = p.disabled ? pal.textDisabled : accent;
        canvas.DrawRoundRect(
            Rect(p.center.x - coreHalf, p.center.y - coreHalf, coreDiam, coreDiam),
            coreHalf, coreCol, Color::Transparent(), 0.0f
        );
        return;
    }

    // 3. Cross-axis fader bar with tactile center indicator notch (DAW / SCADA / CAD)
    if (shape == SliderThumbShape::Bar) {
        const keel::f32 thick = keel::round(keel::max(7.0f, p.thumbSize * 0.58f));
        const keel::f32 span  = keel::round(keel::max(12.0f, p.thumbSize * 1.15f));
        const keel::f32 w = p.vertical ? span : thick;
        const keel::f32 h = p.vertical ? thick : span;
        const Rect barRect(p.center.x - w * 0.5f, p.center.y - h * 0.5f, w, h);

        const bool isBevel = (theme.chrome.frame == FrameTreatment::Bevel3D);
        const bool isBrutal = (theme.chrome.frame == FrameTreatment::HardShadow);
        const bool sharp = isBevel || isBrutal || (p.cornerRadius <= 0.01f);
        const keel::f32 rad = sharp ? 0.0f : keel::min(p.cornerRadius, 2.5f);

        if (isBrutal && !p.disabled && !p.active) {
            const keel::f32 off = p.hovered ? 1.0f : 2.0f;
            const ShadowStyle sh = theme.GetShadow(Elevation::Sm);
            if (sh.color.a > 0u) {
                canvas.DrawRect(Rect(barRect.x + off, barRect.y + off, barRect.w, barRect.h), sh.color);
            }
        }

        const Color barBg = p.disabled ? pal.surface.WithAlphaF(theme.disabledOpacity) :
                            p.active   ? pal.surfaceActive.WithAlpha(255u) :
                            p.hovered  ? pal.surfaceHover.WithAlpha(255u) : pal.surfaceElevated;
        if (isBevel) {
            canvas.DrawRect(barRect, barBg);
            DrawBevelFrame(canvas, barRect, theme.bevel, /*sunken=*/false, /*thin=*/true);
            if (p.focused && !p.disabled) {
                canvas.DrawRoundRect(barRect, 0.0f, Color::Transparent(), pal.focusRing, 1.0f);
            }
        } else {
            const Color barBdr = p.disabled ? pal.border.WithAlphaF(theme.disabledOpacity) :
                                 p.focused  ? pal.focusRing :
                                 (p.active || p.hovered) ? accent :
                                 (isBrutal ? Color::BestTextColor(barBg, pal.textPrimary, pal.background) : pal.borderHover);
            const keel::f32 bw = isBrutal
                ? keel::max(1.5f, 1.5f * keel::max(theme.borderScale, 0.5f))
                : ((p.focused || p.active) ? 1.5f : 1.0f);
            canvas.DrawRoundRect(barRect, rad, barBg, barBdr, bw);
        }

        // Signature center indicator notch perpendicular to travel axis
        const Color notchCol = p.disabled ? pal.textDisabled : accent;
        if (p.vertical) {
            const keel::f32 notchW = keel::max(4.0f, w * 0.52f);
            canvas.DrawRect(Rect(p.center.x - notchW * 0.5f, p.center.y - 0.75f, notchW, 1.5f), notchCol);
        } else {
            const keel::f32 notchH = keel::max(4.0f, h * 0.52f);
            canvas.DrawRect(Rect(p.center.x - 0.75f, p.center.y - notchH * 0.5f, 1.5f, notchH), notchCol);
        }
        return;
    }

    // 4. Circle (Modern) & Square (Brutalist or zero-radius)
    const keel::f32 halfThumb = p.thumbSize * 0.5f;
    const Rect thumbRect(p.center.x - halfThumb, p.center.y - halfThumb, p.thumbSize, p.thumbSize);
    const bool isBrutal = (theme.chrome.frame == FrameTreatment::HardShadow);
    const bool sharp = (shape == SliderThumbShape::Square) || isBrutal || (p.cornerRadius <= 0.01f);

    if (isBrutal && !p.disabled && !p.active) {
        const keel::f32 off = p.hovered ? 1.0f : 2.0f;
        const ShadowStyle sh = theme.GetShadow(Elevation::Sm);
        if (sh.color.a > 0u) {
            canvas.DrawRect(Rect(thumbRect.x + off, thumbRect.y + off, thumbRect.w, thumbRect.h), sh.color);
        }
    }

    const Color thumbBg = p.disabled ? pal.surface.WithAlphaF(theme.disabledOpacity) :
                          p.active   ? accent : pal.surface;
    const Color thumbBorder = p.disabled ? pal.border.WithAlphaF(theme.disabledOpacity) :
                              p.focused  ? pal.focusRing :
                              (p.active || p.hovered) ? accent :
                              (isBrutal ? Color::BestTextColor(thumbBg, pal.textPrimary, pal.background) : pal.borderHover);
    const keel::f32 thumbBorderW = isBrutal
        ? (keel::max(1.5f, 1.5f * keel::max(theme.borderScale, 0.5f)) + ((p.active || p.focused) ? 0.5f : 0.0f))
        : ((p.active || p.hovered || p.focused) ? 2.5f : 2.0f);

    canvas.DrawRoundRect(thumbRect, sharp ? 0.0f : halfThumb, thumbBg, thumbBorder, thumbBorderW);
}

void DefaultStylePainter::DrawProgressBar(
    ICanvas& canvas,
    const UiTheme& theme,
    Rect trackRect,
    keel::f32 ratio,
    bool vertical,
    bool indeterminate,
    keel::f32 animPhase,
    Color trackBg,
    Color trackBorder,
    Color fillColor,
    keel::f32 cornerRadius,
    bool disabled
) const {
    const bool isSeg3D = (theme.chrome.progressFill == ProgressFillStyle::Segmented);
    const bool sharp = isSeg3D || (theme.chrome.frame == FrameTreatment::Bevel3D) || (cornerRadius <= 0.01f);
    const keel::f32 barDim = vertical ? trackRect.w : trackRect.h;

    if (isSeg3D) {
        const Color bg = disabled ? theme.palette.background.WithAlphaF(theme.disabledOpacity) : theme.palette.background;
        canvas.DrawRect(trackRect, bg);
        DrawBevelFrame(canvas, trackRect, theme.bevel, /*sunken=*/true, /*thin=*/false);

        const Rect inner(
            trackRect.x + 2.0f,
            trackRect.y + 2.0f,
            keel::max(0.0f, trackRect.w - 4.0f),
            keel::max(0.0f, trackRect.h - 4.0f)
        );
        if (inner.w < 2.0f || inner.h < 2.0f) return;

        const keel::f32 blockLen = 8.0f;
        const keel::f32 blockGap = 2.0f;
        const keel::f32 stride = blockLen + blockGap;
        canvas.PushClip(inner);

        if (indeterminate) {
            const keel::f32 axisLen = vertical ? inner.h : inner.w;
            const keel::f32 shuttleLen = keel::min(axisLen, blockLen * 3.0f + blockGap * 2.0f);
            const keel::f32 pingPong = 1.0f - keel::abs(keel::fmod(animPhase * 2.0f, 2.0f) - 1.0f);
            const keel::f32 offset = pingPong * keel::max(0.0f, axisLen - shuttleLen);
            for (keel::i32 b = 0; b < 3; ++b) {
                const keel::f32 pos = offset + keel::as<keel::f32>(b) * stride;
                if (vertical) {
                    const keel::f32 by = keel::floor(inner.Bottom() - pos - blockLen);
                    canvas.DrawRect(Rect(inner.x, by, inner.w, blockLen), fillColor);
                } else {
                    const keel::f32 bx = keel::floor(inner.x + pos);
                    canvas.DrawRect(Rect(bx, inner.y, blockLen, inner.h), fillColor);
                }
            }
        } else {
            const keel::f32 axisLen = vertical ? inner.h : inner.w;
            const keel::f32 fillLen = ratio * axisLen;
            for (keel::f32 p = 0.0f; p < fillLen - 0.5f; p += stride) {
                const keel::f32 seg = keel::min(blockLen, fillLen - p);
                if (seg >= 2.0f) {
                    if (vertical) {
                        canvas.DrawRect(Rect(inner.x, keel::floor(inner.Bottom() - p - seg), inner.w, keel::floor(seg)), fillColor);
                    } else {
                        canvas.DrawRect(Rect(keel::floor(inner.x + p), inner.y, keel::floor(seg), inner.h), fillColor);
                    }
                }
            }
        }
        canvas.PopClip();
        return;
    }

    // Modern smooth or sharp bar
    const keel::f32 r = sharp ? 0.0f : (barDim * 0.5f);
    canvas.DrawRoundRect(trackRect, r, trackBg, trackBorder, 1.0f);
    if (indeterminate) {
        const keel::f32 pingPong = 1.0f - keel::abs(keel::fmod(animPhase * 2.0f, 2.0f) - 1.0f);
        const keel::f32 smoothT = pingPong * pingPong * (3.0f - 2.0f * pingPong);
        if (vertical) {
            const keel::f32 shuttleH = keel::max(trackRect.w, trackRect.h * 0.32f);
            const keel::f32 shuttleY = trackRect.y + (1.0f - smoothT) * keel::max(0.0f, trackRect.h - shuttleH);
            canvas.DrawRoundRect(Rect(trackRect.x, shuttleY, trackRect.w, shuttleH), r, fillColor);
        } else {
            const keel::f32 shuttleW = keel::max(trackRect.h, trackRect.w * 0.32f);
            const keel::f32 shuttleX = trackRect.x + smoothT * keel::max(0.0f, trackRect.w - shuttleW);
            canvas.DrawRoundRect(Rect(shuttleX, trackRect.y, shuttleW, trackRect.h), r, fillColor);
        }
    } else if (ratio > 0.001f) {
        if (vertical) {
            const keel::f32 fillH = ratio * trackRect.h;
            if (fillH > 0.5f) {
                canvas.DrawRoundRect(Rect(trackRect.x, trackRect.Bottom() - fillH, trackRect.w, fillH), r, fillColor);
            }
        } else {
            const keel::f32 fillW = ratio * trackRect.w;
            if (fillW > 0.5f) {
                canvas.DrawRoundRect(Rect(trackRect.x, trackRect.y, fillW, trackRect.h), r, fillColor);
            }
        }
    }
}

void DefaultStylePainter::DrawSwitch(
    ICanvas& canvas,
    const UiTheme& theme,
    Rect trackRect,
    keel::f32 knobDiameter,
    keel::f32 knobPadding,
    Color trackFill,
    Color trackStroke,
    keel::f32 strokeWidth,
    Color knobFill,
    bool checked,
    bool focused,
    bool disabled,
    keel::f32 themeRadius
) const {
    const bool isBevel = (theme.chrome.frame == FrameTreatment::Bevel3D);
    const bool sharp = isBevel || (themeRadius <= 0.01f);

    if (isBevel) {
        canvas.DrawRect(trackRect, trackFill);
        DrawBevelFrame(canvas, trackRect, theme.bevel, /*sunken=*/true, /*thin=*/false);
        if (focused && !disabled) {
            canvas.DrawRoundRect(trackRect, 0.0f, Color::Transparent(), theme.palette.focusRing, 1.0f);
        }
    } else {
        canvas.DrawRoundRect(trackRect, sharp ? 0.0f : (trackRect.h * 0.5f), trackFill, trackStroke, strokeWidth);
    }

    const keel::f32 pad = isBevel ? 2.0f : knobPadding;
    const keel::f32 knobH = isBevel ? keel::max(6.0f, trackRect.h - pad * 2.0f) : knobDiameter;
    const keel::f32 knobW = isBevel ? keel::max(10.0f, knobDiameter) : knobDiameter;
    const keel::f32 knobY = trackRect.y + (trackRect.h - knobH) * 0.5f;
    const keel::f32 knobX = checked
        ? (trackRect.x + trackRect.w - knobW - pad)
        : (trackRect.x + pad);

    if (isBevel) {
        const Rect knobRect(keel::round(knobX), keel::round(knobY), keel::round(knobW), keel::round(knobH));
        const Color bevelKnobBg = disabled ? theme.palette.surface.WithAlphaF(theme.disabledOpacity) : theme.palette.surface;
        canvas.DrawRect(knobRect, bevelKnobBg);
        DrawBevelFrame(canvas, knobRect, theme.bevel, /*sunken=*/false, /*thin=*/false);
    } else {
        const Rect knobRect(knobX, knobY, knobW, knobH);
        canvas.DrawRoundRect(knobRect, sharp ? 0.0f : (knobH * 0.5f), knobFill);
    }
}

Color DefaultStylePainter::DrawCheckboxBox(
    ICanvas& canvas,
    const UiTheme& theme,
    Rect boxRect,
    keel::f32 cornerRadius,
    Color fill,
    Color stroke,
    keel::f32 strokeWidth,
    Color defaultMarkColor,
    bool pressed,
    bool focused,
    bool disabled
) const {
    if (theme.chrome.checkControl == CheckControlStyle::SunkenInk) {
        const auto& pal = theme.palette;
        const Color bevelFill = (disabled || pressed) ? pal.surface : pal.inputBackground;
        canvas.DrawRect(boxRect, bevelFill);
        DrawBevelFrame(canvas, boxRect, theme.bevel, /*sunken=*/true, /*thin=*/false);
        if (focused && !disabled) {
            canvas.DrawRoundRect(boxRect, 0.0f, Color::Transparent(), pal.focusRing, 1.0f);
        }
        return disabled ? pal.textDisabled : pal.textPrimary;
    }

    canvas.DrawRoundRect(boxRect, cornerRadius, fill, stroke, strokeWidth);
    return defaultMarkColor;
}

Color DefaultStylePainter::DrawRadioCircle(
    ICanvas& canvas,
    const UiTheme& theme,
    Vec2 center,
    keel::f32 radius,
    Color fill,
    Color stroke,
    keel::f32 strokeWidth,
    Color defaultDotColor,
    bool /*checked*/,
    bool pressed,
    bool focused,
    bool disabled
) const {
    auto circleRect = [](Vec2 c, keel::f32 rad) noexcept -> Rect {
        return Rect(c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f);
    };
    if (theme.chrome.checkControl == CheckControlStyle::SunkenInk) {
        const auto& pal = theme.palette;
        const Color bevelFill = (disabled || pressed) ? pal.surface : pal.inputBackground;
        // Sunken circular well: outer bevel highlight rim + inner dark shadow rim
        canvas.DrawRoundRect(circleRect(center, radius), radius, theme.bevel.shadowInner, theme.bevel.highlightOuter, 1.0f);
        const keel::f32 innerR = keel::max(1.0f, radius - 1.0f);
        canvas.DrawRoundRect(circleRect(Vec2(center.x + 0.5f, center.y + 0.5f), innerR), innerR, bevelFill, theme.bevel.shadowOuter, 1.0f);
        if (focused && !disabled) {
            const keel::f32 focusR = radius + 1.5f;
            canvas.DrawRoundRect(circleRect(center, focusR), focusR, Color::Transparent(), pal.focusRing, 1.0f);
        }
        return disabled ? pal.textDisabled : pal.textPrimary;
    }

    canvas.DrawRoundRect(circleRect(center, radius), radius, fill, stroke, strokeWidth);
    return defaultDotColor;
}

void DefaultStylePainter::DrawTabBarBaseline(
    ICanvas& canvas,
    const UiTheme& theme,
    Rect barRect
) const {
    if (theme.chrome.tabChrome == TabBarChrome::Classic3D) {
        const keel::f32 y = keel::floor(barRect.Bottom() - 1.0f);
        canvas.DrawRect(Rect(barRect.x, y, barRect.w, 1.0f), theme.bevel.highlightOuter);
        return;
    }
    const keel::f32 sepY = barRect.Bottom() - 0.5f;
    canvas.DrawLine(Vec2(barRect.x, sepY), Vec2(barRect.Right(), sepY), theme.palette.borderSubtle, 1.0f);
}

keel::f32 DefaultStylePainter::DrawTabChrome(
    ICanvas& canvas,
    const UiTheme& theme,
    Rect bounds,
    TabChromeStyle style,
    bool active,
    bool hovered,
    bool focused,
    bool disabled,
    bool showSeparator,
    Color accentCol,
    Color pillActiveCol,
    keel::f32 radius
) const {
    const auto& pal = theme.palette;
    if (theme.chrome.tabChrome == TabBarChrome::Classic3D && style != TabChromeStyle::Folder) {
        const BevelPalette& bc = theme.bevel;
        const keel::f32 topOff = active ? 0.0f : 3.0f;
        const keel::f32 tx = keel::floor(bounds.x);
        const keel::f32 ty = keel::floor(bounds.y + topOff);
        const keel::f32 tw = keel::floor(bounds.w);
        const keel::f32 botY = keel::floor(bounds.Bottom());
        const keel::f32 th = keel::max(4.0f, botY - ty);
        const Color tabBg = active
            ? pal.surface
            : ((hovered && !disabled) ? pal.surfaceHover : pal.background);

        if (tw > 4.0f && th > 2.0f) {
            canvas.DrawRect(Rect(tx + 2.0f, ty, tw - 4.0f, 1.0f), tabBg);
            canvas.DrawRect(Rect(tx + 1.0f, ty + 1.0f, tw - 2.0f, 1.0f), tabBg);
            canvas.DrawRect(Rect(tx, ty + 2.0f, tw, th - 2.0f), tabBg);
        } else {
            canvas.DrawRect(Rect(tx, ty, tw, th), tabBg);
        }

        // Top & left raised bevel with 2px chamfered top-left corner
        canvas.DrawRect(Rect(tx + 2.0f, ty, keel::max(0.0f, tw - 4.0f), 1.0f), bc.highlightOuter);
        canvas.DrawRect(Rect(tx + 1.0f, ty + 1.0f, 1.0f, 1.0f), bc.highlightOuter);
        canvas.DrawRect(Rect(tx, ty + 2.0f, 1.0f, keel::max(0.0f, th - 2.0f)), bc.highlightOuter);

        canvas.DrawRect(Rect(tx + 2.0f, ty + 1.0f, keel::max(0.0f, tw - 4.0f), 1.0f), bc.highlightInner);
        canvas.DrawRect(Rect(tx + 1.0f, ty + 2.0f, 1.0f, keel::max(0.0f, th - 2.0f)), bc.highlightInner);

        // Right raised bevel with 2px chamfered top-right corner
        canvas.DrawRect(Rect(tx + tw - 2.0f, ty + 1.0f, 1.0f, 1.0f), bc.shadowOuter);
        canvas.DrawRect(Rect(tx + tw - 1.0f, ty + 2.0f, 1.0f, keel::max(0.0f, th - 2.0f)), bc.shadowOuter);
        canvas.DrawRect(Rect(tx + tw - 2.0f, ty + 2.0f, 1.0f, keel::max(0.0f, th - 2.0f)), bc.shadowInner);

        if (active) {
            canvas.DrawRect(Rect(tx + 2.0f, botY - 1.0f, keel::max(0.0f, tw - 4.0f), 1.0f), tabBg);
        } else if (showSeparator) {
            canvas.DrawRect(Rect(tx, botY - 1.0f, tw, 1.0f), bc.highlightOuter);
        }

        if (focused && !disabled && tw > 8.0f && th > 8.0f) {
            canvas.DrawRoundRect(Rect(tx + 3.0f, ty + 3.0f, tw - 6.0f, th - 5.0f), 0.0f, Color::Transparent(), pal.focusRing, 1.0f);
        }
        return active ? -0.5f : 1.5f;
    }

    // Modern TabBar chrome
    if (style == TabChromeStyle::Underline) {
        if (showSeparator) {
            canvas.DrawLine(Vec2(bounds.x, bounds.Bottom() - 0.5f), Vec2(bounds.Right(), bounds.Bottom() - 0.5f), pal.borderSubtle, 1.0f);
        }
        if (hovered && !active && !disabled) {
            const Rect hoverR(bounds.x, bounds.y + 4.0f, bounds.w, bounds.h - 8.0f);
            canvas.DrawRoundRect(hoverR, radius, pal.surfaceHover);
        }
        if (active) {
            const keel::f32 lineH = 3.0f;
            const keel::f32 linePad = 2.0f;
            const Rect lineR(bounds.x + linePad, bounds.Bottom() - lineH, keel::max(0.0f, bounds.w - linePad * 2.0f), lineH);
            canvas.DrawRoundRect(lineR, radius > 0.01f ? 1.5f : 0.0f, accentCol);
        }
        if (focused && !disabled) {
            const keel::f32 bottomInset = active ? 6.0f : 4.0f;
            canvas.DrawRoundRect(Rect(bounds.x + 1.0f, bounds.y + 2.0f, keel::max(0.0f, bounds.w - 2.0f), keel::max(0.0f, bounds.h - bottomInset)), radius, Color::Transparent(), pal.focusRing, 1.5f);
        }
    } else if (style == TabChromeStyle::Card) {
        const Rect cardR(bounds.x, bounds.y + 2.0f, bounds.w, bounds.h + radius);
        if (active) {
            canvas.DrawRoundRect(cardR, radius, pal.surface, pal.border, 1.0f);
            canvas.DrawRect(Rect(bounds.x + 1.0f, bounds.Bottom() - 1.5f, keel::max(0.0f, bounds.w - 2.0f), 2.0f), pal.surface);
            canvas.DrawRoundRect(Rect(bounds.x + 1.0f, bounds.y + 2.0f, keel::max(0.0f, bounds.w - 2.0f), 2.5f), radius > 0.01f ? 1.0f : 0.0f, accentCol);
        } else {
            if (showSeparator) {
                canvas.DrawLine(Vec2(bounds.x, bounds.Bottom() - 0.5f), Vec2(bounds.Right(), bounds.Bottom() - 0.5f), pal.borderSubtle, 1.0f);
            }
            if (hovered && !disabled) {
                canvas.DrawRoundRect(cardR, radius, pal.surfaceHover, pal.borderSubtle, 1.0f);
            }
        }
        if (focused && !disabled) {
            const Rect focusCardR(bounds.x + 0.75f, bounds.y + 2.75f, keel::max(0.0f, bounds.w - 1.5f), bounds.h + radius);
            canvas.DrawRoundRect(focusCardR, keel::max(0.0f, radius - 0.5f), Color::Transparent(), pal.focusRing, 1.5f);
        }
    } else if (style == TabChromeStyle::Folder) {
        const keel::f32 slope = keel::min(6.0f, bounds.w * 0.14f);
        const keel::f32 topY = bounds.y + (active ? 1.5f : 4.0f);
        const keel::f32 botY = bounds.Bottom();
        VectorPath folderPath;
        folderPath.MoveTo({bounds.x, botY});
        folderPath.LineTo({bounds.x + slope, topY});
        folderPath.LineTo({bounds.Right() - slope, topY});
        folderPath.LineTo({bounds.Right(), botY});
        folderPath.Close();

        if (active || (hovered && !disabled)) {
            PathPaint p{};
            p.fillColor = active ? pal.surfaceElevated : pal.surfaceHover;
            p.renderFill = true;
            p.strokeColor = active ? pal.border : pal.borderSubtle;
            p.strokeWidthPx = 1.0f;
            p.renderStroke = true;
            canvas.DrawPath(folderPath, p);
        }
        if (active) {
            canvas.DrawRect(Rect(bounds.x + 1.0f, botY - 1.5f, keel::max(0.0f, bounds.w - 2.0f), 2.0f), pal.surfaceElevated);
            canvas.DrawLine(Vec2(bounds.x + slope, topY + 0.5f), Vec2(bounds.Right() - slope, topY + 0.5f), accentCol, 2.0f);
        } else if (showSeparator) {
            canvas.DrawLine(Vec2(bounds.x, botY - 0.5f), Vec2(bounds.Right(), botY - 0.5f), pal.borderSubtle, 1.0f);
        }
        if (focused && !disabled) {
            VectorPath focusPath;
            focusPath.MoveTo({bounds.x + 0.75f, botY});
            focusPath.LineTo({bounds.x + slope, topY + 0.75f});
            focusPath.LineTo({bounds.Right() - slope, topY + 0.75f});
            focusPath.LineTo({bounds.Right() - 0.75f, botY});
            PathPaint fp{};
            fp.renderFill = false;
            fp.strokeColor = pal.focusRing;
            fp.strokeWidthPx = 1.5f;
            fp.renderStroke = true;
            canvas.DrawPath(focusPath, fp);
        }
    } else {
        // Pill
        const Color focusBorder = (focused && !disabled) ? pal.focusRing : Color::Transparent();
        const keel::f32 focusBorderW = (focused && !disabled) ? 1.5f : 0.0f;
        if (active) {
            canvas.DrawRoundRect(bounds, radius, pillActiveCol, focusBorder, focusBorderW);
        } else if (hovered && !disabled) {
            canvas.DrawRoundRect(bounds, radius, pal.surfaceHover, focusBorder, focusBorderW);
        } else if (focused && !disabled) {
            canvas.DrawRoundRect(bounds, radius, Color::Transparent(), focusBorder, focusBorderW);
        }
    }
    return 0.0f;
}

void DefaultStylePainter::DrawMenuBarTrigger(
    ICanvas& canvas,
    const UiTheme& theme,
    Rect rect,
    keel::f32 radius,
    bool hovered,
    bool open,
    bool disabled,
    Color& outTextColor,
    Vec2& outTextShift
) const {
    const auto& pal = theme.palette;
    outTextShift = Vec2{0.0f, 0.0f};

    if (theme.chrome.frame == FrameTreatment::Bevel3D) {
        outTextColor = disabled ? pal.textPrimary.WithAlphaF(theme.disabledOpacity) : pal.textPrimary;
        if (!disabled && open) {
            canvas.DrawRect(rect, pal.accent);
            DrawBevelFrame(canvas, rect, theme.bevel, /*sunken=*/true, /*thin=*/true);
            outTextColor = pal.accentText;
            outTextShift = Vec2{1.0f, 1.0f};
        } else if (!disabled && hovered) {
            DrawBevelFrame(canvas, rect, theme.bevel, /*sunken=*/false, /*thin=*/true);
        }
        return;
    }

    const bool isDark = (theme.mode == ThemeMode::Dark);
    Color bg = Color::Transparent();
    outTextColor = pal.textPrimary;

    if (disabled) {
        outTextColor = outTextColor.WithAlphaF(theme.disabledOpacity);
    } else if (open) {
        bg = isDark ? pal.surfaceActive : pal.accent.WithAlphaF(0.14f);
        outTextColor = pal.accent;
    } else if (hovered) {
        bg = pal.surfaceHover;
    }

    if (bg.a > 0u) {
        if (radius > 0.0f) {
            canvas.DrawRoundRect(rect, radius, bg);
        } else {
            canvas.DrawRect(rect, bg);
        }
    }
}

bool DefaultStylePainter::DrawSelectionHighlight(
    ICanvas& canvas,
    const UiTheme& theme,
    Rect itemRect,
    keel::f32 itemRadius,
    bool hovered,
    bool selected,
    bool subExpanded,
    bool isDanger,
    bool disabled,
    Color dangerColor
) const {
    if (disabled) return false;
    const auto& pal = theme.palette;
    const SelectionHighlightStyle selStyle = theme.chrome.selection;

    if (selStyle == SelectionHighlightStyle::SolidAccent) {
        if (hovered || selected) {
            canvas.DrawRect(itemRect, isDanger ? dangerColor : pal.accent);
            return true;
        }
        return false;
    }

    const bool isDark = (theme.mode == ThemeMode::Dark);
    if (hovered) {
        const Color hovBg = isDanger
            ? dangerColor.WithAlphaF(isDark ? 0.22f : 0.14f)
            : ((selStyle == SelectionHighlightStyle::Tonal && selected && !subExpanded)
                ? pal.accent.WithAlphaF(isDark ? 0.24f : 0.18f)
                : ((subExpanded || selected) ? pal.surfaceActive : pal.surfaceHover));
        if (itemRadius > 0.0f) {
            canvas.DrawRoundRect(itemRect, itemRadius, hovBg, Color::Transparent(), 0.0f);
        } else {
            canvas.DrawRect(itemRect, hovBg);
        }
    } else if (selected) {
        const Color selBg = isDanger
            ? dangerColor.WithAlphaF(isDark ? 0.16f : 0.10f)
            : ((selStyle == SelectionHighlightStyle::Tonal)
                ? pal.accent.WithAlphaF(isDark ? 0.16f : 0.12f)
                : pal.surfaceHover);
        if (itemRadius > 0.0f) {
            canvas.DrawRoundRect(itemRect, itemRadius, selBg, Color::Transparent(), 0.0f);
        } else {
            canvas.DrawRect(itemRect, selBg);
        }
    }
    if (selStyle == SelectionHighlightStyle::LeftAccentBar && (selected || hovered) && itemRect.h > 8.0f) {
        const keel::f32 barH = keel::max(8.0f, itemRect.h * 0.52f);
        const Rect barR(itemRect.x + 1.0f, itemRect.y + (itemRect.h - barH) * 0.5f, 3.0f, barH);
        const Color barCol = isDanger ? dangerColor : pal.accent;
        canvas.DrawRoundRect(barR, 1.5f, selected ? barCol : barCol.WithAlphaF(0.65f));
    }
    return false;
}

void DefaultStylePainter::DrawBadge(
    ICanvas& canvas,
    const UiTheme& theme,
    Rect rect,
    Color bg,
    Color border,
    keel::f32 borderWidth,
    keel::f32 fallbackRadius
) const {
    if (rect.w <= 0.0f || rect.h <= 0.0f) return;

    if (theme.chrome.frame == FrameTreatment::Bevel3D) {
        const Rect bevelRect = SnapBevelRect(rect);
        if (bg.a > 0u) {
            canvas.DrawRect(bevelRect, bg);
        }
        DrawBevelFrame(canvas, bevelRect, theme.bevel, /*sunken=*/true, /*thin=*/true);
        return;
    }

    keel::f32 r = fallbackRadius;
    if (theme.chrome.badgeShape == BadgeShape::Sharp) {
        r = 0.0f;
    } else if (theme.chrome.badgeShape == BadgeShape::Rounded) {
        r = keel::min(fallbackRadius, 4.0f);
    }
    canvas.DrawRoundRect(rect, r, bg, border, borderWidth);
}

namespace {

void DrawScrollbarArrowGlyph(
    ICanvas& canvas,
    Rect btnRect,
    bool horizontal,
    bool isStart,
    ScrollbarArrowStyle style,
    Color color,
    keel::f32 shift
) {
    if (style == ScrollbarArrowStyle::None || color.a == 0u) return;
    const keel::f32 cx = keel::floor(btnRect.x + btnRect.w * 0.5f) + 0.5f + shift;
    const keel::f32 cy = keel::floor(btnRect.y + btnRect.h * 0.5f) + 0.5f + shift;
    const keel::f32 dim = keel::min(btnRect.w, btnRect.h);
    const keel::f32 dir = isStart ? -1.0f : 1.0f;

    if (style == ScrollbarArrowStyle::Triangle) {
        const keel::f32 halfBase = keel::clamp(keel::floor(dim * 0.25f), 2.5f, 5.0f);
        const keel::f32 tipOff = keel::clamp(keel::floor(dim * 0.16f), 2.0f, 3.5f);
        VectorPath tri;
        if (horizontal) {
            tri.MoveTo({cx + dir * tipOff, cy});
            tri.LineTo({cx - dir * tipOff, cy - halfBase});
            tri.LineTo({cx - dir * tipOff, cy + halfBase});
        } else {
            tri.MoveTo({cx, cy + dir * tipOff});
            tri.LineTo({cx - halfBase, cy - dir * tipOff});
            tri.LineTo({cx + halfBase, cy - dir * tipOff});
        }
        tri.Close();
        PathPaint pp{};
        pp.fillColor = color;
        pp.renderFill = true;
        pp.renderStroke = false;
        canvas.DrawPath(tri, pp);
    } else if (style == ScrollbarArrowStyle::Chevron) {
        const keel::f32 halfW = keel::clamp(dim * 0.22f, 2.5f, 4.5f);
        const keel::f32 depth = keel::clamp(dim * 0.14f, 1.8f, 3.2f);
        const keel::f32 strokeW = keel::clamp(dim * 0.10f, 1.3f, 2.0f);
        if (horizontal) {
            const Vec2 tip(cx + dir * depth, cy);
            canvas.DrawLine(Vec2(cx - dir * depth, cy - halfW), tip, color, strokeW);
            canvas.DrawLine(Vec2(cx - dir * depth, cy + halfW), tip, color, strokeW);
        } else {
            const Vec2 tip(cx, cy + dir * depth);
            canvas.DrawLine(Vec2(cx - halfW, cy - dir * depth), tip, color, strokeW);
            canvas.DrawLine(Vec2(cx + halfW, cy - dir * depth), tip, color, strokeW);
        }
    }
}

[[nodiscard]] Color ResolveScrollbarTrackBg(
    const UiTheme& theme,
    ScrollbarChromeStyle chromeStyle
) noexcept {
    const auto& pal = theme.palette;
    if (chromeStyle == ScrollbarChromeStyle::ClassicBevel) {
        const Color classicTrack = (theme.mode == ThemeMode::Dark)
            ? Color::Lerp(pal.background, pal.surface, 0.35f)
            : Color::Lerp(pal.surface, theme.bevel.highlightInner, 0.55f);
        return (Color::ContrastRatio(classicTrack, pal.surface) >= 1.08f)
            ? classicTrack
            : pal.controlTrack;
    }
    return pal.controlTrack;
}

[[nodiscard]] Color ResolveFramedScrollbarBorder(
    const ThemePalette& pal,
    Color trackBg
) noexcept {
    return (Color::ContrastRatio(pal.border, trackBg) >= 1.25f)
        ? pal.border
        : pal.borderHover;
}

[[nodiscard]] keel::f32 ResolveFramedScrollbarLineWidth(const UiTheme& theme) noexcept {
    return keel::max(1.0f, 1.5f * keel::max(theme.borderScale, 0.5f));
}

} // namespace

void DefaultStylePainter::DrawScrollbar(
    ICanvas& canvas,
    const UiTheme& theme,
    const ScrollbarPaintParams& p
) const {
    if (p.trackRect.w <= 0.0f || p.trackRect.h <= 0.0f) return;

    const auto& pal = theme.palette;
    const ScrollbarStyleTokens& sb = theme.scrollbarStyle;
    const bool isFramed = (sb.chrome == ScrollbarChromeStyle::Framed);
    const Color accent = (p.accentColor.a > 0u) ? p.accentColor : pal.accent;
    const Color trackBg = ResolveScrollbarTrackBg(theme, sb.chrome);
    const Color railBorder = isFramed ? ResolveFramedScrollbarBorder(pal, trackBg) : Color::Transparent();
    const keel::f32 framedLineW = isFramed ? ResolveFramedScrollbarLineWidth(theme) : 0.0f;

    // 1. Track background
    const bool drawTrack = sb.alwaysShowTrack || p.hovered || p.dragging || (sb.chrome != ScrollbarChromeStyle::Modern);
    if (drawTrack) {
        if (sb.chrome == ScrollbarChromeStyle::Modern) {
            canvas.DrawRoundRect(p.trackRect, p.cornerRadius, trackBg, Color::Transparent(), 0.0f);
        } else if (sb.chrome == ScrollbarChromeStyle::Flat) {
            canvas.DrawRect(p.trackRect, trackBg);
        } else if (sb.chrome == ScrollbarChromeStyle::ClassicBevel) {
            canvas.DrawRect(p.trackRect, trackBg);
            const Color edgeTint = theme.bevel.shadowInner.WithAlphaF(0.20f);
            if (p.horizontal) {
                canvas.DrawRect(Rect(p.trackRect.x, p.trackRect.y, p.trackRect.w, 1.0f), edgeTint);
            } else {
                canvas.DrawRect(Rect(p.trackRect.x, p.trackRect.y, 1.0f, p.trackRect.h), edgeTint);
            }
        } else {
            // ScrollbarChromeStyle::Framed (Brutalist): solid rail + content-side bold divider
            canvas.DrawRect(p.trackRect, trackBg);
            if (p.horizontal) {
                canvas.DrawRect(Rect(p.trackRect.x, p.trackRect.y, p.trackRect.w, framedLineW), railBorder);
            } else {
                canvas.DrawRect(Rect(p.trackRect.x, p.trackRect.y, framedLineW, p.trackRect.h), railBorder);
            }
        }
    }

    // 2. Thumb knob
    if (p.thumbRect.w > 0.0f && p.thumbRect.h > 0.0f) {
        const keel::f32 inset = keel::max(0.0f, sb.thumbInset);
        Rect thumbR = p.thumbRect;
        if (isFramed) {
            // Keep thumb cleanly inside the rail interior past the content-side divider
            if (p.horizontal && thumbR.h > framedLineW + 2.0f) {
                thumbR.y += framedLineW;
                thumbR.h -= framedLineW;
            } else if (!p.horizontal && thumbR.w > framedLineW + 2.0f) {
                thumbR.x += framedLineW;
                thumbR.w -= framedLineW;
            }
        }
        if (inset > 0.0f) {
            if (p.horizontal && thumbR.h > inset * 2.0f + 2.0f) {
                thumbR.y += inset;
                thumbR.h -= inset * 2.0f;
            } else if (!p.horizontal && thumbR.w > inset * 2.0f + 2.0f) {
                thumbR.x += inset;
                thumbR.w -= inset * 2.0f;
            }
        }

        const bool thumbHot = p.thumbHovered || (p.hovered && !p.startHovered && !p.endHovered);
        if (sb.chrome == ScrollbarChromeStyle::ClassicBevel) {
            const Color thumbCol = p.dragging
                ? pal.surfaceActive.WithAlpha(255u)
                : (thumbHot ? pal.surfaceHover.WithAlpha(255u) : pal.surface);
            canvas.DrawRect(thumbR, thumbCol);
            DrawBevelFrame(canvas, thumbR, theme.bevel, /*sunken=*/false, /*thin=*/false);
        } else {
            const Color thumbCol = p.dragging
                ? accent
                : (thumbHot ? pal.textSecondary : pal.textMuted);
            if (sb.chrome == ScrollbarChromeStyle::Modern) {
                canvas.DrawRoundRect(thumbR, p.cornerRadius, thumbCol, Color::Transparent(), 0.0f);
            } else if (sb.chrome == ScrollbarChromeStyle::Flat) {
                canvas.DrawRect(thumbR, thumbCol);
            } else {
                // ScrollbarChromeStyle::Framed (Brutalist): high-contrast fill + crisp 0dp border
                const Color thumbBorder = (Color::ContrastRatio(pal.borderHover, thumbCol) >= 1.35f)
                    ? pal.borderHover
                    : Color::BestTextColor(thumbCol, pal.textPrimary, pal.background);
                canvas.DrawRoundRect(thumbR, 0.0f, thumbCol, thumbBorder, keel::min(framedLineW, 1.5f));
            }
        }
    }

    // 3. Optional stepper arrow buttons (Start = Up/Left, End = Down/Right)
    if (p.hasButtons) {
        auto drawBtn = [&](Rect r, bool isStart, bool hov, bool press) {
            if (r.w <= 1.0f || r.h <= 1.0f) return;
            const Color btnBg = (sb.chrome == ScrollbarChromeStyle::Flat)
                ? (press ? accent : (hov ? pal.controlTrackHover : trackBg))
                : (press ? pal.surfaceActive.WithAlpha(255u) : (hov ? pal.surfaceHover.WithAlpha(255u) : pal.surface));
            const Color arrowCol = Color::BestTextColor(btnBg, pal.textPrimary, pal.accentText);

            Rect glyphRect = r;
            if (sb.chrome == ScrollbarChromeStyle::ClassicBevel) {
                canvas.DrawRect(r, btnBg);
                DrawBevelFrame(canvas, r, theme.bevel, /*sunken=*/press, /*thin=*/false);
            } else if (isFramed) {
                canvas.DrawRect(r, btnBg);
                // Continuous content-side rail divider + junction divider separating button from track
                if (p.horizontal) {
                    canvas.DrawRect(Rect(r.x, r.y, r.w, framedLineW), railBorder);
                    const keel::f32 seamX = isStart ? (r.Right() - framedLineW) : r.x;
                    canvas.DrawRect(Rect(seamX, r.y, framedLineW, r.h), railBorder);
                    if (r.h > framedLineW + 2.0f) {
                        glyphRect.y += framedLineW;
                        glyphRect.h -= framedLineW;
                    }
                } else {
                    canvas.DrawRect(Rect(r.x, r.y, framedLineW, r.h), railBorder);
                    const keel::f32 seamY = isStart ? (r.Bottom() - framedLineW) : r.y;
                    canvas.DrawRect(Rect(r.x, seamY, r.w, framedLineW), railBorder);
                    if (r.w > framedLineW + 2.0f) {
                        glyphRect.x += framedLineW;
                        glyphRect.w -= framedLineW;
                    }
                }
            } else {
                canvas.DrawRect(r, btnBg);
            }

            const keel::f32 shift = (press && sb.chrome != ScrollbarChromeStyle::Flat)
                ? theme.chrome.buttonPressOffset
                : 0.0f;
            DrawScrollbarArrowGlyph(canvas, glyphRect, p.horizontal, isStart, sb.arrowStyle, arrowCol, shift);
        };

        drawBtn(p.startButtonRect, true, p.startHovered, p.startPressed);
        drawBtn(p.endButtonRect, false, p.endHovered, p.endPressed);
    }
}

void DefaultStylePainter::DrawScrollbarCorner(
    ICanvas& canvas,
    const UiTheme& theme,
    Rect cornerRect
) const {
    if (cornerRect.w <= 0.0f || cornerRect.h <= 0.0f) return;
    const ScrollbarStyleTokens& sb = theme.scrollbarStyle;
    if (!sb.showCornerBox) return;

    const auto& pal = theme.palette;
    if (sb.chrome == ScrollbarChromeStyle::ClassicBevel) {
        canvas.DrawRect(cornerRect, pal.surface);
        return;
    }

    const Color trackBg = ResolveScrollbarTrackBg(theme, sb.chrome);
    canvas.DrawRect(cornerRect, trackBg);
    if (sb.chrome == ScrollbarChromeStyle::Framed) {
        const Color railBorder = ResolveFramedScrollbarBorder(pal, trackBg);
        const keel::f32 framedLineW = ResolveFramedScrollbarLineWidth(theme);
        canvas.DrawRect(Rect(cornerRect.x, cornerRect.y, cornerRect.w, framedLineW), railBorder);
        canvas.DrawRect(Rect(cornerRect.x, cornerRect.y, framedLineW, cornerRect.h), railBorder);
    }
}

void DefaultStylePainter::DrawSplitSash(
    ICanvas& canvas,
    const UiTheme& theme,
    Rect sashRect,
    bool verticalBar,
    bool showGrip,
    bool hovered,
    bool dragging,
    bool disabled
) const {
    if (sashRect.w <= 0.0f || sashRect.h <= 0.0f) return;

    const auto& pal = theme.palette;
    const bool active = !disabled && (hovered || dragging);
    const keel::f32 thick = verticalBar ? sashRect.w : sashRect.h;
    const keel::f32 span = verticalBar ? sashRect.h : sashRect.w;

    // 1. Classic 3D (Bevel3D / Etched3D): raised 3D splitter bar + embossed 3D grip dots
    if (theme.chrome.frame == FrameTreatment::Bevel3D || theme.chrome.divider == DividerStyle::Etched3D) {
        const Color baseBg = disabled
            ? pal.surface
            : (dragging ? pal.surfaceActive : (hovered ? pal.surfaceHover : pal.surface));
        canvas.DrawRect(sashRect, baseBg);

        if (thick < 4.0f) {
            if (verticalBar) {
                const keel::f32 x = keel::floor(sashRect.Center().x - 1.0f);
                DrawBevelDividerV(canvas, x, sashRect.y, sashRect.Bottom(), theme.bevel);
            } else {
                const keel::f32 y = keel::floor(sashRect.Center().y - 1.0f);
                DrawBevelDividerH(canvas, sashRect.x, sashRect.Right(), y, theme.bevel);
            }
        } else {
            const bool thinBevel = (thick < 8.0f);
            DrawBevelFrame(canvas, sashRect, theme.bevel, /*sunken=*/(dragging && !disabled), thinBevel);

            if (showGrip && span >= 24.0f) {
                const keel::f32 cx = keel::floor(sashRect.x) + keel::floor(keel::floor(sashRect.w) * 0.5f);
                const keel::f32 cy = keel::floor(sashRect.y) + keel::floor(keel::floor(sashRect.h) * 0.5f);
                const Color dotShadow = disabled
                    ? theme.bevel.shadowInner.WithAlphaF(theme.disabledOpacity)
                    : theme.bevel.shadowInner;
                const Color dotHi = disabled
                    ? theme.bevel.highlightOuter.WithAlphaF(theme.disabledOpacity)
                    : theme.bevel.highlightOuter;

                for (keel::i32 i = -2; i <= 2; ++i) {
                    const keel::f32 off = keel::as<keel::f32>(i) * 4.0f;
                    if (verticalBar) {
                        const keel::f32 dy = cy + off;
                        canvas.DrawRect(Rect(cx - 1.0f, dy - 1.0f, 1.0f, 1.0f), dotShadow);
                        canvas.DrawRect(Rect(cx,        dy,        1.0f, 1.0f), dotHi);
                    } else {
                        const keel::f32 dx = cx + off;
                        canvas.DrawRect(Rect(dx - 1.0f, cy - 1.0f, 1.0f, 1.0f), dotShadow);
                        canvas.DrawRect(Rect(dx,        cy,        1.0f, 1.0f), dotHi);
                    }
                }
            }
        }
        return;
    }

    // 2. Brutalist (HardShadow / Bold) & Modern / Fluent: shared exact-center track, divider, and grip
    const bool isBrutalist =
        (theme.chrome.frame == FrameTreatment::HardShadow || theme.chrome.divider == DividerStyle::Bold);
    const Color accentCol = pal.accent;
    const Vec2 center = sashRect.Center();

    const Color trackBg = active
        ? accentCol.WithAlphaF(isBrutalist ? (dragging ? 0.30f : 0.20f) : (dragging ? 0.32f : 0.25f))
        : (isBrutalist ? pal.surface : pal.surfaceHover.WithAlphaF(0.35f));
    canvas.DrawRect(sashRect, trackBg);

    const keel::f32 lineT = isBrutalist ? keel::min(2.0f, thick) : 1.0f;
    const Color lineCol = active
        ? (isBrutalist ? accentCol : accentCol.WithAlphaF(0.85f))
        : (isBrutalist ? pal.border : pal.borderSubtle);
    const Rect lineRect = verticalBar
        ? Rect(center.x - lineT * 0.5f, sashRect.y, lineT, sashRect.h)
        : Rect(sashRect.x, center.y - lineT * 0.5f, sashRect.w, lineT);
    canvas.DrawRect(lineRect, lineCol);

    if (showGrip) {
        const keel::f32 gripT = isBrutalist
            ? ((thick >= 4.0f) ? 4.0f : lineT)
            : ((thick >= 3.0f) ? 3.0f : thick);
        const keel::f32 gripLen = 24.0f;
        const keel::f32 gripW = verticalBar ? gripT : gripLen;
        const keel::f32 gripH = verticalBar ? gripLen : gripT;
        if (sashRect.w >= (verticalBar ? keel::min(2.0f, gripW) : gripW) &&
            sashRect.h >= (verticalBar ? gripH : keel::min(2.0f, gripH)) &&
            thick >= (isBrutalist ? gripT : 2.0f)) {
            const Rect gripRect(center.x - gripW * 0.5f, center.y - gripH * 0.5f, gripW, gripH);
            const Color gripCol = active
                ? accentCol
                : (isBrutalist ? pal.textPrimary : pal.textMuted.WithAlphaF(0.65f));
            if (isBrutalist) {
                canvas.DrawRect(gripRect, gripCol);
            } else {
                canvas.DrawRoundRect(gripRect, keel::min(gripW, gripH) * 0.5f, gripCol, Color::Transparent(), 0.0f);
            }
        }
    }
}

const IStylePainter& GetDefaultStylePainter() noexcept {
    static const DefaultStylePainter kDefaultPainter{};
    return kDefaultPainter;
}

} // namespace guikit
