#include "guikit/style/style_painter.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/pixel_grid.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/math.h"

namespace guikit {

// ==============================================================================
// Reusable 3D Bevel Primitives (Windows 98 EDGE_RAISED / EDGE_SUNKEN / EDGE_ETCHED)
// ==============================================================================

namespace {

[[nodiscard]] Color ResolveContrastInk(Color bg, Color preferredInk, const BevelPalette& bevel) noexcept {
    return (Color::ContrastRatio(bg, preferredInk) >= 4.5f)
        ? preferredInk
        : Color::BestTextColor(bg, bevel.shadowOuter, bevel.highlightOuter);
}

[[nodiscard]] BevelPalette DeriveBevelPalette(Color base, const BevelPalette& ref) noexcept {
    const keel::u8 a = base.a;
    return BevelPalette{
        .highlightOuter = ref.highlightOuter.WithAlpha(a),
        .highlightInner = Color::Lerp(base, ref.highlightOuter, 0.35f).WithAlpha(a),
        .shadowInner    = base,
        .shadowOuter    = Color::Lerp(base, ref.shadowOuter, 0.65f).WithAlpha(a)
    };
}

void DrawCustomBevelFrame(
    ICanvas& canvas,
    Rect r,
    Color baseColor,
    const BevelPalette& refBevel,
    keel::f32 borderWidth,
    bool sunken,
    bool etched
) {
    if (borderWidth <= 0.0f || baseColor.a == 0u) {
        return;
    }
    const PixelGrid pg = PixelGrid::From(canvas);
    const PixelGrid::SnappedPxRect s = pg.SnapRectPx(r);
    const keel::i32 step = pg.UnitStepPx();
    const keel::i32 wPx = s.Width();
    const keel::i32 hPx = s.Height();
    if (wPx < step * 3 || hPx < step * 3) {
        return;
    }
    const BevelPalette tinted = DeriveBevelPalette(baseColor, refBevel);
    const keel::i32 x0 = s.x0;
    const keel::i32 y0 = s.y0;
    const keel::i32 x1 = s.x1;
    const keel::i32 y1 = s.y1;

    if (etched) {
        if (borderWidth < 1.0f) {
            DefaultStylePainter::DrawBevelFrame(canvas, pg.PxRectToLogical(s), tinted, /*sunken=*/true, /*thin=*/true);
            return;
        }
        if (wPx < step * 4 || hPx < step * 4) {
            return;
        }
        const keel::i32 maxGPx = keel::max(step, (keel::min(wPx, hPx) / 2) - step);
        const keel::i32 gPx = keel::min(keel::max(step, pg.StrokePx(borderWidth)), maxGPx);

        // Groove of exact integer thickness gPx in baseColor, paired with crisp step-px theme highlight edge
        canvas.DrawRect(pg.PxRectToLogical(x0, y0, x1 - step, y0 + gPx), baseColor);
        canvas.DrawRect(pg.PxRectToLogical(x0, y1 - step - gPx, x1 - step, y1 - step), baseColor);
        if (hPx - step > gPx * 2) {
            canvas.DrawRect(pg.PxRectToLogical(x0, y0 + gPx, x0 + gPx, y1 - step - gPx), baseColor);
            canvas.DrawRect(pg.PxRectToLogical(x1 - step - gPx, y0 + gPx, x1 - step, y1 - step - gPx), baseColor);
        }

        canvas.DrawRect(pg.PxRectToLogical(x0, y1 - step, x1, y1), tinted.highlightOuter);
        canvas.DrawRect(pg.PxRectToLogical(x1 - step, y0, x1, y1), tinted.highlightOuter);
        if (wPx > gPx * 2 + step && hPx > gPx * 2 + step) {
            canvas.DrawRect(pg.PxRectToLogical(x0 + gPx, y0 + gPx, x1 - gPx - step, y0 + gPx + step), tinted.highlightOuter);
            canvas.DrawRect(pg.PxRectToLogical(x0 + gPx, y0 + gPx, x0 + gPx + step, y1 - gPx - step), tinted.highlightOuter);
        }
        return;
    }

    const keel::f32 bwDp = keel::max(1.0f, keel::round(borderWidth));
    if (bwDp <= 1.5f) {
        DefaultStylePainter::DrawBevelFrame(canvas, pg.PxRectToLogical(s), tinted, sunken, /*thin=*/borderWidth < 1.0f);
        return;
    }

    // Thick 3D raised/sunken frame (bwDp >= 2): solid base body of exact integer thickness + crisp step-px light/shadow edges
    const keel::i32 bwPx = keel::max(step * 2, pg.StrokePx(borderWidth));
    const keel::i32 effBw = keel::min(bwPx, keel::min(wPx, hPx) / 2);
    canvas.DrawRect(pg.PxRectToLogical(x0, y0, x1, y0 + effBw), baseColor);
    canvas.DrawRect(pg.PxRectToLogical(x0, y1 - effBw, x1, y1), baseColor);
    if (hPx > effBw * 2) {
        canvas.DrawRect(pg.PxRectToLogical(x0, y0 + effBw, x0 + effBw, y1 - effBw), baseColor);
        canvas.DrawRect(pg.PxRectToLogical(x1 - effBw, y0 + effBw, x1, y1 - effBw), baseColor);
    }

    const Color tlOuter = sunken ? tinted.shadowOuter    : tinted.highlightOuter;
    const Color brOuter = sunken ? tinted.highlightOuter : tinted.shadowOuter;
    canvas.DrawRect(pg.PxRectToLogical(x0, y0, x1 - step, y0 + step), tlOuter);
    canvas.DrawRect(pg.PxRectToLogical(x0, y0, x0 + step, y1 - step), tlOuter);
    canvas.DrawRect(pg.PxRectToLogical(x0, y1 - step, x1, y1), brOuter);
    canvas.DrawRect(pg.PxRectToLogical(x1 - step, y0, x1, y1), brOuter);

    if (wPx > effBw * 2 + step && hPx > effBw * 2 + step) {
        const keel::i32 inOff = effBw - step;
        const Color tlInner = sunken ? tinted.shadowInner    : tinted.highlightInner;
        const Color brInner = sunken ? tinted.highlightInner : tinted.shadowInner;
        canvas.DrawRect(pg.PxRectToLogical(x0 + inOff, y0 + inOff, x1 - inOff - step, y0 + effBw), tlInner);
        canvas.DrawRect(pg.PxRectToLogical(x0 + inOff, y0 + inOff, x0 + effBw, y1 - inOff - step), tlInner);
        canvas.DrawRect(pg.PxRectToLogical(x0 + inOff, y1 - effBw, x1 - inOff, y1 - inOff), brInner);
        canvas.DrawRect(pg.PxRectToLogical(x1 - effBw, y0 + inOff, x1 - inOff, y1 - inOff), brInner);
    }
}

} // namespace

void DefaultStylePainter::DrawBevelFrame(
    ICanvas& canvas,
    Rect r,
    const BevelPalette& c,
    bool sunken,
    bool thin
) {
    const PixelGrid pg = PixelGrid::From(canvas);
    const PixelGrid::SnappedPxRect s = pg.SnapRectPx(r);
    const keel::i32 step = pg.UnitStepPx();
    const keel::i32 wPx = s.Width();
    const keel::i32 hPx = s.Height();
    if (wPx < step * 3 || hPx < step * 3) return;

    const keel::i32 x0 = s.x0;
    const keel::i32 y0 = s.y0;
    const keel::i32 x1 = s.x1;
    const keel::i32 y1 = s.y1;

    const Color tlOuter = sunken ? c.shadowInner    : c.highlightOuter;
    const Color brOuter = sunken ? c.highlightOuter : c.shadowOuter;

    canvas.DrawRect(pg.PxRectToLogical(x0, y0, x1 - step, y0 + step), tlOuter);
    canvas.DrawRect(pg.PxRectToLogical(x0, y0, x0 + step, y1 - step), tlOuter);
    canvas.DrawRect(pg.PxRectToLogical(x0, y1 - step, x1, y1), brOuter);
    canvas.DrawRect(pg.PxRectToLogical(x1 - step, y0, x1, y1), brOuter);

    if (thin || wPx < step * 5 || hPx < step * 5) {
        return;
    }

    const Color tlInner = sunken ? c.shadowOuter    : c.highlightInner;
    const Color brInner = sunken ? c.highlightInner : c.shadowInner;
    const keel::i32 s2 = step * 2;
    canvas.DrawRect(pg.PxRectToLogical(x0 + step, y0 + step, x1 - s2, y0 + s2), tlInner);
    canvas.DrawRect(pg.PxRectToLogical(x0 + step, y0 + step, x0 + s2, y1 - s2), tlInner);
    canvas.DrawRect(pg.PxRectToLogical(x0 + step, y1 - s2, x1 - step, y1 - step), brInner);
    canvas.DrawRect(pg.PxRectToLogical(x1 - s2, y0 + step, x1 - step, y1 - step), brInner);
}

void DefaultStylePainter::DrawBevelEtchedFrame(
    ICanvas& canvas,
    Rect r,
    const BevelPalette& c
) {
    const PixelGrid pg = PixelGrid::From(canvas);
    const PixelGrid::SnappedPxRect s = pg.SnapRectPx(r);
    const keel::i32 step = pg.UnitStepPx();
    if (s.Width() < step * 4 || s.Height() < step * 4) return;

    const keel::i32 x0 = s.x0;
    const keel::i32 y0 = s.y0;
    const keel::i32 x1 = s.x1;
    const keel::i32 y1 = s.y1;
    const keel::i32 s2 = step * 2;

    canvas.DrawRect(pg.PxRectToLogical(x0 + step, y0 + step, x1 - step, y0 + s2), c.highlightOuter);
    canvas.DrawRect(pg.PxRectToLogical(x0 + step, y0 + step, x0 + s2, y1 - step), c.highlightOuter);
    canvas.DrawRect(pg.PxRectToLogical(x0, y1 - step, x1, y1), c.highlightOuter);
    canvas.DrawRect(pg.PxRectToLogical(x1 - step, y0, x1, y1), c.highlightOuter);

    canvas.DrawRect(pg.PxRectToLogical(x0, y0, x1 - step, y0 + step), c.shadowInner);
    canvas.DrawRect(pg.PxRectToLogical(x0, y0, x0 + step, y1 - step), c.shadowInner);
    canvas.DrawRect(pg.PxRectToLogical(x0 + step, y1 - s2, x1 - step, y1 - step), c.shadowInner);
    canvas.DrawRect(pg.PxRectToLogical(x1 - s2, y0 + step, x1 - step, y1 - step), c.shadowInner);
}

void DefaultStylePainter::DrawBevelDividerH(
    ICanvas& canvas,
    keel::f32 x0,
    keel::f32 x1,
    keel::f32 y,
    const BevelPalette& c
) {
    const PixelGrid pg = PixelGrid::From(canvas);
    const keel::i32 px0 = pg.SnapXToPx(x0);
    const keel::i32 px1 = pg.SnapXToPx(x1);
    if (px1 <= px0) return;
    const keel::i32 py = pg.SnapYToPx(y);
    const keel::i32 step = pg.UnitStepPx();
    canvas.DrawRect(pg.PxRectToLogical(px0, py, px1, py + step), c.shadowInner);
    canvas.DrawRect(pg.PxRectToLogical(px0, py + step, px1, py + step * 2), c.highlightOuter);
}

void DefaultStylePainter::DrawBevelDividerV(
    ICanvas& canvas,
    keel::f32 x,
    keel::f32 y0,
    keel::f32 y1,
    const BevelPalette& c
) {
    const PixelGrid pg = PixelGrid::From(canvas);
    const keel::i32 py0 = pg.SnapYToPx(y0);
    const keel::i32 py1 = pg.SnapYToPx(y1);
    if (py1 <= py0) return;
    const keel::i32 px = pg.SnapXToPx(x);
    const keel::i32 step = pg.UnitStepPx();
    canvas.DrawRect(pg.PxRectToLogical(px, py0, px + step, py1), c.shadowInner);
    canvas.DrawRect(pg.PxRectToLogical(px + step, py0, px + step * 2, py1), c.highlightOuter);
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
    const PixelGrid pg = PixelGrid::From(canvas);
    const PixelGrid::SnappedPxRect sp = pg.SnapRectPx(r);
    const keel::i32 step = pg.UnitStepPx();
    const keel::f32 stepDp = pg.PxToDp(step);
    const keel::f32 inset = keel::clamp(radius * 0.65f, stepDp, r.w * 0.25f);
    const keel::f32 x0 = pg.SnapEdgeX(r.x + inset);
    const keel::f32 x1 = pg.SnapEdgeX(r.Right() - inset);
    if (x1 <= x0) return;

    const bool isDark = (theme.mode == ThemeMode::Dark);
    if (!pressed) {
        const Color hi = theme.bevel.highlightOuter.WithAlphaF(isDark ? 0.14f : 0.45f);
        const keel::f32 topY = pg.PxToLogicalY(sp.y0) + stepDp * 0.5f;
        canvas.DrawLine(Vec2(x0, topY), Vec2(x1, topY), hi, stepDp);
    }
    if (includeBottomShade) {
        const Color lo = theme.bevel.shadowOuter.WithAlphaF(isDark ? 0.34f : 0.14f);
        const keel::f32 botY = pg.PxToLogicalY(sp.y1) - stepDp * 0.5f;
        canvas.DrawLine(Vec2(x0, botY), Vec2(x1, botY), lo, stepDp);
    }
}

/// Estimates chroma [0..1] of an sRGB color to distinguish saturated semantic tints from neutral surfaces.
[[nodiscard]] keel::f32 EstimateColorChroma(Color c) noexcept {
    const keel::u8 hi = keel::max(c.r, keel::max(c.g, c.b));
    const keel::u8 lo = keel::min(c.r, keel::min(c.g, c.b));
    return keel::as<keel::f32>(hi - lo) / 255.0f;
}

/// Derives palette-pure `GlassParams` for container surfaces (`SurfaceRole::Flat..DockEdge`).
[[nodiscard]] GlassParams BuildSurfaceGlassParams(
    const UiTheme& theme,
    const SurfacePaintParams& p
) noexcept {
    const bool isDark = (theme.mode == ThemeMode::Dark);
    const keel::f32 srcA = keel::as<keel::f32>(p.bg.a) / 255.0f;
    GlassParams gp{};
    gp.rimColor = theme.bevel.highlightOuter;
    gp.borderWidth = 0.0f; // Drawn in DrawSurfaceFrame on top of clipped children
    gp.borderColor = Color::Transparent();

    keel::f32 targetAlpha = isDark ? 0.42f : 0.56f;
    switch (p.role) {
        case SurfaceRole::Popup:
            targetAlpha   = isDark ? 0.70f : 0.80f;
            gp.blurRadius = 18.0f;
            gp.frost      = 0.86f;
            gp.refraction = 0.30f;
            gp.dispersion = 0.16f;
            gp.rimLight   = isDark ? 0.50f : 0.65f;
            gp.saturation = 1.24f;
            break;
        case SurfaceRole::Raised:
            targetAlpha   = isDark ? 0.46f : 0.58f;
            gp.blurRadius = 15.0f;
            gp.frost      = 0.72f;
            gp.refraction = 0.38f;
            gp.dispersion = 0.22f;
            gp.rimLight   = isDark ? 0.55f : 0.70f;
            gp.saturation = 1.28f;
            break;
        case SurfaceRole::DockEdge:
            targetAlpha   = isDark ? 0.54f : 0.66f;
            gp.blurRadius = 14.0f;
            gp.frost      = 0.78f;
            gp.refraction = 0.22f;
            gp.dispersion = 0.12f;
            gp.rimLight   = isDark ? 0.36f : 0.52f;
            gp.saturation = 1.20f;
            break;
        case SurfaceRole::Sunken:
            targetAlpha   = isDark ? 0.52f : 0.62f;
            gp.blurRadius = 10.0f;
            gp.frost      = 0.82f;
            gp.refraction = 0.14f;
            gp.dispersion = 0.06f;
            gp.rimLight   = isDark ? 0.18f : 0.32f;
            gp.saturation = 1.12f;
            break;
        case SurfaceRole::Flat:
        default:
            targetAlpha   = isDark ? 0.40f : 0.54f;
            gp.blurRadius = 12.0f;
            gp.frost      = 0.70f;
            gp.refraction = 0.32f;
            gp.dispersion = 0.18f;
            gp.rimLight   = isDark ? 0.42f : 0.58f;
            gp.saturation = 1.22f;
            break;
    }

    // Preserve explicit low-alpha translucency if caller intentionally dimmed p.bg
    const keel::f32 effAlpha = (srcA < 0.99f) ? keel::min(srcA, targetAlpha) : targetAlpha;
    gp.tint = p.bg.WithAlphaF(effAlpha);

    if (p.bgGradient != nullptr && p.bgGradient->IsValid()) {
        const Gradient& g = *p.bgGradient;
        const Color c0 = g.stops[0].color;
        const Color c1 = g.stops[(g.stopCount > 1u) ? (g.stopCount - 1u) : 0u].color;
        const keel::f32 a0 = (keel::as<keel::f32>(c0.a) / 255.0f < 0.99f) ? keel::min(keel::as<keel::f32>(c0.a) / 255.0f, targetAlpha) : targetAlpha;
        const keel::f32 a1 = (keel::as<keel::f32>(c1.a) / 255.0f < 0.99f) ? keel::min(keel::as<keel::f32>(c1.a) / 255.0f, targetAlpha) : targetAlpha;
        gp.tint = c0.WithAlphaF(a0);
        gp.tintEnd = c1.WithAlphaF(a1);
        gp.tintRadial = (g.kind == GradientKind::Radial);
        if (!gp.tintRadial) {
            const keel::f32 dx = g.end.x - g.start.x;
            const keel::f32 dy = g.end.y - g.start.y;
            gp.tintAngleDeg = keel::rad_to_deg(keel::atan2(dx, -dy));
        }
    } else if (theme.fillStyle == FillStyle::Gradient || p.role == SurfaceRole::Raised || p.role == SurfaceRole::Popup) {
        const keel::f32 topLift = isDark ? 0.055f : 0.040f;
        const keel::f32 botDrop = isDark ? -0.035f : -0.025f;
        const Color cTop = p.bg.ShiftLightness(topLift);
        const Color cBot = (theme.fillStyle == FillStyle::Gradient)
            ? Color::Lerp(p.bg.ShiftLightness(botDrop), theme.palette.accent, isDark ? 0.14f : 0.08f)
            : p.bg.ShiftLightness(botDrop);
        gp.tint = cTop.WithAlphaF(effAlpha);
        gp.tintEnd = cBot.WithAlphaF(keel::min(0.92f, effAlpha + 0.05f));
        gp.tintAngleDeg = 145.0f;
        if (!p.explicitBorder && p.border.a > 0u) {
            gp.borderColor = Color::Lerp(p.border, theme.bevel.highlightOuter, isDark ? 0.44f : 0.65f)
                .WithAlphaF(isDark ? 0.65f : 0.80f);
            gp.borderColorEnd = p.border.WithAlphaF(isDark ? 0.34f : 0.48f);
        }
    }
    return gp;
}

/// Builds a subtle studio tonal gradient for `ChromeStyle::Luminous` (`FrameTreatment::GradientSurface`) surfaces.
[[nodiscard]] Gradient BuildLuminousSurfaceGradient(
    const UiTheme& theme,
    SurfaceRole role,
    Color base
) noexcept {
    const bool isDark = (theme.mode == ThemeMode::Dark);
    if (role == SurfaceRole::Sunken) {
        return Gradient::Linear(
            base.ShiftLightness(isDark ? -0.022f : -0.018f),
            base.ShiftLightness(isDark ? 0.016f : 0.012f),
            180.0f
        );
    }
    if (role == SurfaceRole::Raised || role == SurfaceRole::Popup) {
        const Color c0 = base.ShiftLightness(isDark ? 0.048f : 0.022f);
        const Color c1 = base.ShiftLightness(isDark ? -0.024f : -0.022f);
        return Gradient::Linear(c0, c1, 165.0f);
    }
    const Color c0 = base.ShiftLightness(isDark ? 0.026f : 0.014f);
    const Color c1 = base.ShiftLightness(isDark ? -0.014f : -0.014f);
    return Gradient::Linear(c0, c1, 180.0f);
}

/// Builds a directional top-left luminous 1px border gradient for studio cards and controls.
[[nodiscard]] Gradient BuildLuminousBorderGradient(
    const UiTheme& theme,
    Color baseBorder,
    bool emphasize
) noexcept {
    const bool isDark = (theme.mode == ThemeMode::Dark);
    const Color topLeft = Color::Lerp(
        baseBorder,
        theme.bevel.highlightOuter,
        emphasize ? (isDark ? 0.62f : 0.78f) : (isDark ? 0.42f : 0.60f)
    ).WithAlphaF(isDark ? 0.84f : 0.92f);
    const Color botRight = baseBorder.WithAlphaF(isDark ? 0.40f : 0.60f);
    return Gradient::Linear(topLeft, botRight, 135.0f);
}

/// Renders a subtle dual-corner studio ambient backdrop (palette-pure: top-left accent + bottom-right secondary accent)
/// behind Floating shells, Luminous cards, and Glass panels without any hardcoded colors.
void DrawAmbientStudioBackdrop(ICanvas& canvas, const UiTheme& theme, Rect rect, Color baseBg) {
    const bool isDark = (theme.mode == ThemeMode::Dark);
    canvas.DrawRect(rect, baseBg);

    const Color glowA = theme.palette.accent.WithAlphaF(isDark ? 0.11f : 0.065f);
    const Color glowB = theme.GetSemanticAccent(UiVariant::Success).WithAlphaF(isDark ? 0.085f : 0.055f);
    const Gradient cornerA = Gradient::Radial(glowA, glowA.WithAlpha(0u), Vec2{0.12f, 0.08f}, 1.25f);
    const Gradient cornerB = Gradient::Radial(glowB, glowB.WithAlpha(0u), Vec2{0.90f, 0.92f}, 1.20f);
    canvas.DrawGradientRoundRect(rect, 0.0f, cornerA, Color::Transparent(), 0.0f);
    canvas.DrawGradientRoundRect(rect, 0.0f, cornerB, Color::Transparent(), 0.0f);
}

/// Derives palette-pure `GlassParams` for interactive controls (`Button`, `Input`, `DropdownButton`).
[[nodiscard]] GlassParams BuildFrameGlassParams(
    const UiTheme& theme,
    FrameKind kind,
    const FramePaintParams& p,
    const Gradient* activeFillGrad = nullptr
) noexcept {
    const bool isDark = (theme.mode == ThemeMode::Dark);
    const auto& pal = theme.palette;
    const keel::f32 srcA = keel::as<keel::f32>(p.bg.a) / 255.0f;
    const keel::f32 chroma = EstimateColorChroma(p.bg);
    const bool isSemanticFill = (chroma >= 0.22f) && (srcA >= 0.75f);

    GlassParams gp{};
    gp.rimColor = theme.bevel.highlightOuter;

    const Color defaultGlassBorder = Color::Lerp(
        (p.border.a > 0u) ? p.border : pal.border,
        theme.bevel.highlightOuter,
        isDark ? 0.24f : 0.42f
    );
    gp.borderColor = (p.explicitBorder || p.focused || p.hasStatusAccent)
        ? p.border
        : defaultGlassBorder.WithAlphaF(p.disabled ? theme.disabledOpacity : (isDark ? 0.55f : 0.72f));
    gp.borderWidth = (p.borderWidth > 0.0f)
        ? p.borderWidth
        : (1.0f * keel::max(theme.borderScale, 0.5f));

    if (kind == FrameKind::Input) {
        const keel::f32 baseA = isDark ? 0.54f : 0.68f;
        gp.tint       = p.bg.WithAlphaF(keel::min(srcA, baseA));
        gp.blurRadius = 10.0f;
        gp.frost      = 0.84f;
        gp.refraction = 0.14f;
        gp.dispersion = 0.06f;
        gp.rimLight   = p.focused && !p.disabled ? 0.46f : 0.22f;
        gp.saturation = 1.12f;
        return gp;
    }

    // Button / DropdownButton / ToolbarButton / Keycap
    const bool hasGrad = (activeFillGrad != nullptr && activeFillGrad->IsValid());
    keel::f32 baseA = (isSemanticFill || hasGrad)
        ? (isDark ? 0.78f : 0.82f)
        : (isDark ? 0.38f : 0.54f);
    if (!p.disabled) {
        if (p.pressed || p.active) baseA = keel::min(0.92f, baseA + 0.10f);
        else if (p.hovered)        baseA = keel::min(0.88f, baseA + 0.06f);
    }
    const keel::f32 effA = (srcA < 0.75f && !hasGrad) ? srcA : baseA;
    if (hasGrad) {
        const Color c0 = activeFillGrad->stops[0].color;
        const Color c1 = activeFillGrad->stops[(activeFillGrad->stopCount > 1u) ? (activeFillGrad->stopCount - 1u) : 0u].color;
        gp.tint = c0.WithAlphaF(effA);
        gp.tintEnd = c1.WithAlphaF(effA);
        gp.tintRadial = (activeFillGrad->kind == GradientKind::Radial);
        if (!gp.tintRadial) {
            const keel::f32 dx = activeFillGrad->end.x - activeFillGrad->start.x;
            const keel::f32 dy = activeFillGrad->end.y - activeFillGrad->start.y;
            gp.tintAngleDeg = keel::rad_to_deg(keel::atan2(dx, -dy));
        }
        if (!p.explicitBorder && !p.focused) {
            gp.borderColor = Color::Lerp(c0, theme.bevel.highlightOuter, 0.48f).WithAlphaF(0.75f);
            gp.borderColorEnd = Color::Lerp(c1, theme.bevel.highlightOuter, 0.16f).WithAlphaF(0.45f);
        }
    } else {
        gp.tint = p.bg.WithAlphaF(effA);
    }
    gp.blurRadius = 11.0f;
    gp.frost      = (isSemanticFill || hasGrad) ? 0.68f : 0.58f;
    gp.refraction = p.disabled ? 0.12f : (p.hovered || p.active ? 0.44f : 0.34f);
    gp.dispersion = p.disabled ? 0.05f : (p.hovered || p.active ? 0.24f : 0.18f);
    gp.rimLight   = p.disabled ? 0.18f : (p.hovered || p.active ? 0.68f : 0.48f);
    gp.saturation = 1.26f;
    return gp;
}

} // namespace

void DefaultStylePainter::DrawSurfaceBg(
    ICanvas& canvas,
    const UiTheme& theme,
    const SurfacePaintParams& p
) const {
    if (p.rect.w <= 0.0f || p.rect.h <= 0.0f) return;

    const PixelGrid pg = PixelGrid::From(canvas);
    const Rect surfRect = pg.SnapRect(p.rect);
    const bool roundedOverride = p.explicitRadius && (p.radius > 0.01f);
    const bool useBevel = (theme.chrome.frame == FrameTreatment::Bevel3D) && !roundedOverride;
    const keel::f32 effRadius = useBevel ? 0.0f : p.radius;
    const bool hasExplicitGrad = (p.bgGradient != nullptr && p.bgGradient->HasVisibleColor());

    if (p.bg.a > 0u || hasExplicitGrad) {
        Elevation effElev = p.elevation;
        if (effElev == Elevation::None) {
            if (p.role == SurfaceRole::Raised) effElev = Elevation::Md;
            else if (p.role == SurfaceRole::Popup) effElev = Elevation::Lg;
        }
        DrawShadowForRect(canvas, theme, surfRect, effRadius, effElev, p.shadowScale);

        const bool isViewportBackdrop =
            !hasExplicitGrad &&
            (p.role == SurfaceRole::Flat) &&
            (p.bg == theme.palette.background) &&
            (effRadius <= 0.01f) &&
            (p.borderWidth <= 0.0f);
        if (theme.chrome.frame == FrameTreatment::Glass && p.role != SurfaceRole::None) {
            if (isViewportBackdrop) {
                DrawAmbientStudioBackdrop(canvas, theme, surfRect, p.bg);
                return;
            }
            const GlassParams gp = BuildSurfaceGlassParams(theme, p);
            canvas.DrawGlassRect(surfRect, effRadius, gp);
            return;
        }

        if (hasExplicitGrad) {
            canvas.DrawGradientRoundRect(surfRect, effRadius, *p.bgGradient, Color::Transparent(), 0.0f);
            return;
        }

        if (theme.chrome.frame == FrameTreatment::GradientSurface && p.role != SurfaceRole::None) {
            if (isViewportBackdrop) {
                DrawAmbientStudioBackdrop(canvas, theme, surfRect, p.bg);
            } else {
                const Gradient surfGrad = BuildLuminousSurfaceGradient(theme, p.role, p.bg);
                canvas.DrawGradientRoundRect(surfRect, effRadius, surfGrad, Color::Transparent(), 0.0f);
            }
            return;
        }

        if (effRadius > 0.0f) {
            canvas.DrawRoundRect(surfRect, effRadius, p.bg, Color::Transparent(), 0.0f);
        } else {
            canvas.DrawRect(surfRect, p.bg);
        }
    }
}

void DefaultStylePainter::DrawSurfaceFrame(
    ICanvas& canvas,
    const UiTheme& theme,
    const SurfacePaintParams& p
) const {
    if (p.rect.w <= 0.0f || p.rect.h <= 0.0f || p.role == SurfaceRole::None) return;

    const PixelGrid pg = PixelGrid::From(canvas);
    const Rect r = pg.SnapRect(p.rect);
    const bool roundedOverride = p.explicitRadius && (p.radius > 0.01f);
    const bool useBevel = (theme.chrome.frame == FrameTreatment::Bevel3D) && !roundedOverride;

    // Docked single-edge seam for shell panels (Sidebar, Toolbar, MenuBar, BottomPanel, StatusBar)
    if (p.role == SurfaceRole::DockEdge && p.dockSeam != DockSeam::None && p.radius <= 0.01f) {
        if (p.borderWidth <= 0.0f || p.border.a == 0u) return;
        const bool isEtchedChrome = (theme.chrome.divider == DividerStyle::Etched3D);
        const bool thinSeam = (p.borderWidth < 1.0f);
        const keel::i32 step = pg.UnitStepPx();
        const keel::f32 seamThickness = (isEtchedChrome && !thinSeam)
            ? keel::max(pg.PxToDp(step * 2), pg.StrokeDp(p.borderWidth))
            : pg.StrokeDp(p.borderWidth);
        auto drawEdge = [&](Vec2 a, Vec2 b) {
            const bool horiz = (keel::abs(a.y - b.y) < 0.01f);
            if (isEtchedChrome && !thinSeam) {
                const BevelPalette seamPal = p.explicitBorder
                    ? DeriveBevelPalette(p.border, theme.bevel)
                    : theme.bevel;
                if (p.borderWidth <= 2.01f) {
                    if (horiz) {
                        DrawBevelDividerH(canvas, a.x, b.x, a.y, seamPal);
                    } else {
                        DrawBevelDividerV(canvas, a.x, a.y, b.y, seamPal);
                    }
                } else {
                    const keel::i32 thickPx = keel::max(step * 2, pg.StrokePx(p.borderWidth));
                    const keel::i32 ax = pg.SnapXToPx(keel::min(a.x, b.x));
                    const keel::i32 bx = pg.SnapXToPx(keel::max(a.x, b.x));
                    const keel::i32 ay = pg.SnapYToPx(keel::min(a.y, b.y));
                    const keel::i32 by = pg.SnapYToPx(keel::max(a.y, b.y));
                    if (horiz) {
                        canvas.DrawRect(pg.PxRectToLogical(ax, ay, bx, ay + thickPx), p.border);
                        canvas.DrawRect(pg.PxRectToLogical(ax, ay, bx, ay + step), seamPal.shadowOuter);
                        canvas.DrawRect(pg.PxRectToLogical(ax, ay + thickPx - step, bx, ay + thickPx), seamPal.highlightOuter);
                    } else {
                        canvas.DrawRect(pg.PxRectToLogical(ax, ay, ax + thickPx, by), p.border);
                        canvas.DrawRect(pg.PxRectToLogical(ax, ay, ax + step, by), seamPal.shadowOuter);
                        canvas.DrawRect(pg.PxRectToLogical(ax + thickPx - step, ay, ax + thickPx, by), seamPal.highlightOuter);
                    }
                }
            } else if (!isEtchedChrome) {
                DrawDivider(canvas, theme, a, b, p.border, p.borderWidth);
            } else {
                const Rect lineRect = horiz
                    ? Rect(keel::min(a.x, b.x), a.y, keel::abs(b.x - a.x), seamThickness)
                    : Rect(a.x, keel::min(a.y, b.y), seamThickness, keel::abs(b.y - a.y));
                canvas.DrawRect(pg.SnapRect(lineRect), p.border);
            }
        };
        switch (p.dockSeam) {
            case DockSeam::Left:
                drawEdge(Vec2(r.x, r.y), Vec2(r.x, r.Bottom()));
                return;
            case DockSeam::Right: {
                const keel::f32 x = keel::max(r.x, r.Right() - seamThickness);
                drawEdge(Vec2(x, r.y), Vec2(x, r.Bottom()));
                return;
            }
            case DockSeam::Top:
                drawEdge(Vec2(r.x, r.y), Vec2(r.Right(), r.y));
                return;
            case DockSeam::Bottom: {
                const keel::f32 y = keel::max(r.y, r.Bottom() - seamThickness);
                drawEdge(Vec2(r.x, y), Vec2(r.Right(), y));
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
        const bool hasCustomBevel = p.explicitBorder && p.borderWidth > 0.0f && p.border.a > 0u;
        const bool thinBevel = (p.borderWidth > 0.0f && p.borderWidth < 1.0f);
        switch (p.role) {
            case SurfaceRole::Flat:
                if (hasCustomBevel) {
                    DrawCustomBevelFrame(canvas, r, p.border, theme.bevel, p.borderWidth, /*sunken=*/false, /*etched=*/true);
                } else if (thinBevel) {
                    DrawBevelFrame(canvas, r, theme.bevel, /*sunken=*/true, /*thin=*/true);
                } else {
                    DrawBevelEtchedFrame(canvas, r, theme.bevel);
                }
                return;
            case SurfaceRole::Sunken:
                if (hasCustomBevel) {
                    DrawCustomBevelFrame(canvas, r, p.border, theme.bevel, p.borderWidth, /*sunken=*/true, /*etched=*/false);
                } else {
                    DrawBevelFrame(canvas, r, theme.bevel, /*sunken=*/true, thinBevel);
                }
                return;
            case SurfaceRole::Raised:
            case SurfaceRole::Popup:
            case SurfaceRole::DockEdge:
                if (hasCustomBevel) {
                    DrawCustomBevelFrame(canvas, r, p.border, theme.bevel, p.borderWidth, /*sunken=*/false, /*etched=*/false);
                } else {
                    DrawBevelFrame(canvas, r, theme.bevel, /*sunken=*/false, thinBevel);
                }
                return;
            case SurfaceRole::None:
                return;
        }
    }

    const bool hasBorderGrad = (p.borderGradient != nullptr && p.borderGradient->HasVisibleColor());
    if (p.borderWidth > 0.0f && (p.border.a > 0u || hasBorderGrad)) {
        const keel::f32 rawBw = (theme.chrome.frame == FrameTreatment::HardShadow)
            ? keel::max(p.borderWidth, 1.5f * keel::max(theme.borderScale, 0.5f))
            : p.borderWidth;
        const keel::f32 bw = pg.StrokeDp(rawBw);
        if (hasBorderGrad) {
            canvas.DrawGradientRoundRect(r, p.radius, Gradient{}, p.border, bw, p.borderGradient);
        } else if (theme.chrome.frame == FrameTreatment::GradientSurface && !p.explicitBorder) {
            const Gradient lumBdr = BuildLuminousBorderGradient(
                theme,
                p.border,
                p.role == SurfaceRole::Raised || p.role == SurfaceRole::Popup
            );
            canvas.DrawGradientRoundRect(r, p.radius, Gradient{}, p.border, bw, &lumBdr);
        } else {
            Color strokeCol = p.border;
            if (theme.chrome.frame == FrameTreatment::Glass && !p.explicitBorder) {
                const bool isDark = (theme.mode == ThemeMode::Dark);
                strokeCol = Color::Lerp(p.border, theme.bevel.highlightOuter, isDark ? 0.24f : 0.45f)
                    .WithAlphaF(isDark ? 0.52f : 0.70f);
            }
            canvas.DrawRoundRect(r, p.radius, Color::Transparent(), strokeCol, bw);
        }
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

    const PixelGrid pg = PixelGrid::From(canvas);
    const Rect frameRect = pg.SnapRect(p.rect);
    const bool isInput = (kind == FrameKind::Input);
    const bool roundedOverride = p.explicitRadius && (p.radius > 0.01f);
    const bool useBevel = isInput
        ? ((theme.chrome.field == FieldTreatment::Sunken3D) && !roundedOverride)
        : ((theme.chrome.frame == FrameTreatment::Bevel3D) && !roundedOverride);
    keel::f32 effRadius = useBevel ? 0.0f : p.radius;
    if (!useBevel && kind == FrameKind::Keycap) {
        if (theme.chrome.badgeShape == BadgeShape::Sharp) {
            effRadius = 0.0f;
        } else if (theme.chrome.badgeShape == BadgeShape::Rounded) {
            effRadius = keel::min(effRadius, 4.0f);
        }
    }

    Elevation effElev = p.elevation;
    if (effElev == Elevation::None && kind == FrameKind::Button &&
        theme.chrome.frame == FrameTreatment::HardShadow &&
        !p.disabled && !p.pressed && !p.active && p.bg.a > 0u) {
        effElev = Elevation::Sm;
    }
    DrawShadowForRect(canvas, theme, frameRect, effRadius, effElev, p.shadowScale);

    // Non-Bevel path: Flat (Modern), Directional1px (Fluent), HardShadow (Brutalist), or rounded override
    if (!useBevel) {
        if (isInput && theme.chrome.field == FieldTreatment::BottomAccent) {
            const Color baseBorder = p.disabled
                ? p.border
                : theme.palette.border;
            const keel::f32 baseBw = pg.StrokeDp((p.borderWidth > 0.0f) ? keel::min(p.borderWidth, 1.0f) : 1.0f);
            canvas.DrawRoundRect(frameRect, effRadius, p.bg, baseBorder, baseBw);

            if (frameRect.w > 6.0f && frameRect.h > 6.0f) {
                const keel::f32 inset = keel::clamp(effRadius * 0.55f, 1.0f, frameRect.w * 0.25f);
                const keel::f32 x0 = frameRect.x + inset;
                const keel::f32 x1 = frameRect.Right() - inset;
                if (x1 > x0) {
                    const Color accentBar = p.focused && !p.disabled
                        ? ((p.accent.a > 0u) ? p.accent : theme.palette.focusRing)
                        : (p.disabled ? theme.palette.border.WithAlphaF(theme.disabledOpacity) : theme.palette.borderHover);
                    const keel::i32 barPx = pg.StrokePx((p.focused && !p.disabled) ? 2.0f : 1.0f);
                    const keel::f32 barH = pg.PxToDp(barPx);
                    const keel::f32 barY = frameRect.Bottom() - barH * 0.5f;
                    canvas.DrawLine(Vec2(x0, barY), Vec2(x1, barY), accentBar, barH);
                }
            }
            return;
        }

        const bool hasExplicitBgGrad = (p.bgGradient != nullptr && p.bgGradient->HasVisibleColor());
        const bool hasExplicitBdrGrad = (p.borderGradient != nullptr && p.borderGradient->HasVisibleColor());
        const bool isSemanticSolid = !isInput && (p.bg.a >= 180u) && (EstimateColorChroma(p.bg) >= 0.22f);
        const bool autoGradFill = !hasExplicitBgGrad && (theme.fillStyle == FillStyle::Gradient) && isSemanticSolid;
        const bool isLuminousSurface = (theme.chrome.frame == FrameTreatment::GradientSurface) && !isInput && (p.bg.a > 0u);

        Gradient derivedBgGrad{};
        const Gradient* effBgGrad = nullptr;
        if (hasExplicitBgGrad) {
            effBgGrad = p.bgGradient;
        } else if (autoGradFill) {
            derivedBgGrad = theme.DeriveAccentGradient(p.bg, 135.0f);
            if (p.disabled) {
                derivedBgGrad = derivedBgGrad.WithAlphaF(theme.disabledOpacity);
            }
            effBgGrad = &derivedBgGrad;
        } else if (isLuminousSurface) {
            const bool isDark = (theme.mode == ThemeMode::Dark);
            const Color topC = p.bg.ShiftLightness(isDark ? 0.042f : 0.025f);
            const Color botC = p.bg.ShiftLightness(isDark ? -0.028f : -0.024f);
            derivedBgGrad = Gradient::Linear(topC, botC, 180.0f);
            effBgGrad = &derivedBgGrad;
        }

        // Subtle colored ambient glow on luminous / gradient semantic buttons
        if (!p.disabled && (autoGradFill || (isLuminousSurface && isSemanticSolid) || hasExplicitBgGrad) &&
            (kind == FrameKind::Button || kind == FrameKind::DropdownButton) &&
            theme.shadowsEnabled && !p.pressed) {
            const Color glowSeed = effBgGrad ? effBgGrad->AverageColor() : p.bg;
            if (glowSeed.a >= 160u && EstimateColorChroma(glowSeed) >= 0.18f) {
                const bool isDark = (theme.mode == ThemeMode::Dark);
                const keel::f32 glowA = p.hovered ? (isDark ? 0.34f : 0.24f) : (isDark ? 0.20f : 0.14f);
                canvas.DrawShadow(p.rect, effRadius, p.hovered ? 12.0f : 8.0f, Vec2{0.0f, 3.0f}, glowSeed.WithAlphaF(glowA), 0.0f);
            }
        }

        if (theme.chrome.frame == FrameTreatment::Glass && (p.bg.a > 0u || effBgGrad != nullptr)) {
            const GlassParams gp = BuildFrameGlassParams(theme, kind, p, effBgGrad);
            canvas.DrawGlassRect(frameRect, effRadius, gp);
            return;
        }

        Gradient derivedBdrGrad{};
        const Gradient* effBdrGrad = nullptr;
        keel::f32 effBw = p.borderWidth;
        Color effBdrCol = p.border;
        if (hasExplicitBdrGrad) {
            effBdrGrad = p.borderGradient;
        } else if ((isLuminousSurface || autoGradFill) && !p.explicitBorder && !p.focused && !p.hasStatusAccent &&
                   theme.chrome.frame != FrameTreatment::HardShadow) {
            const Color bdrSeed = (p.border.a > 0u) ? p.border : (effBgGrad ? effBgGrad->stops[0].color : theme.palette.border);
            derivedBdrGrad = BuildLuminousBorderGradient(theme, bdrSeed, p.hovered || p.active || isSemanticSolid);
            effBdrGrad = &derivedBdrGrad;
            if (effBw <= 0.0f) {
                effBw = 1.0f * keel::max(theme.borderScale, 0.5f);
            }
            effBdrCol = bdrSeed;
        }
        const keel::f32 snappedBw = (effBw > 0.0f) ? pg.StrokeDp(effBw) : 0.0f;

        if (effBgGrad != nullptr || effBdrGrad != nullptr) {
            const Gradient fillArg = (effBgGrad != nullptr)
                ? *effBgGrad
                : ((p.bg.a > 0u) ? Gradient::Linear(p.bg, p.bg, 180.0f) : Gradient{});
            canvas.DrawGradientRoundRect(frameRect, effRadius, fillArg, effBdrCol, snappedBw, effBdrGrad);
            return;
        }

        if (effRadius > 0.0f) {
            canvas.DrawRoundRect(frameRect, effRadius, p.bg, p.border, snappedBw);
        } else if (snappedBw > 0.0f && p.border.a > 0u) {
            canvas.DrawRoundRect(frameRect, 0.0f, p.bg, p.border, snappedBw);
        } else if (p.bg.a > 0u) {
            canvas.DrawRect(frameRect, p.bg);
        }

        if (theme.chrome.frame == FrameTreatment::Directional1px && !p.disabled && p.bg.a > 0u &&
            (kind == FrameKind::Button || kind == FrameKind::DropdownButton)) {
            DrawDirectionalSheen(canvas, theme, frameRect, effRadius, p.pressed || p.active, /*includeBottomShade=*/true);
        }
        return;
    }

    // FrameTreatment::Bevel3D / FieldTreatment::Sunken3D path
    const PixelGrid::SnappedPxRect bevelPx = pg.SnapRectPx(p.rect);
    const Rect bevelRect = pg.PxRectToLogical(bevelPx);
    const keel::i32 step = pg.UnitStepPx();
    const keel::f32 stepDp = pg.PxToDp(step);
    const Color effBg = (isInput && p.disabled && p.bg.a > 0u) ? theme.palette.surface : p.bg;
    if (effBg.a > 0u) {
        canvas.DrawRect(bevelRect, effBg);
    }

    const bool hasCustomBevel = p.explicitBorder && p.borderWidth > 0.0f && p.border.a > 0u;
    const bool thinBevel = (p.borderWidth > 0.0f && p.borderWidth < 1.0f);
    switch (kind) {
        case FrameKind::Input:
            if (hasCustomBevel) {
                DrawCustomBevelFrame(canvas, bevelRect, p.border, theme.bevel, p.borderWidth, /*sunken=*/true, /*etched=*/false);
            } else {
                DrawBevelFrame(canvas, bevelRect, theme.bevel, /*sunken=*/true, thinBevel);
            }
            if ((p.focused || p.hasStatusAccent) && !p.disabled && bevelPx.Width() > step * 6 && bevelPx.Height() > step * 6) {
                const keel::i32 ringInsetPx = hasCustomBevel
                    ? keel::max(step * 2, pg.StrokePx(p.borderWidth))
                    : (thinBevel ? step : (step * 2));
                if (bevelPx.Width() > ringInsetPx * 2 + step * 2 && bevelPx.Height() > ringInsetPx * 2 + step * 2) {
                    canvas.DrawRoundRect(
                        pg.PxRectToLogical(
                            bevelPx.x0 + ringInsetPx,
                            bevelPx.y0 + ringInsetPx,
                            bevelPx.x1 - ringInsetPx,
                            bevelPx.y1 - ringInsetPx
                        ),
                        0.0f, Color::Transparent(), (p.accent.a > 0u) ? p.accent : theme.palette.focusRing, stepDp
                    );
                }
            }
            break;

        case FrameKind::Button:
        case FrameKind::DropdownButton:
            if (hasCustomBevel) {
                DrawCustomBevelFrame(
                    canvas, bevelRect, p.border, theme.bevel, p.borderWidth,
                    /*sunken=*/(!p.disabled && (p.pressed || p.active)),
                    /*etched=*/false
                );
            } else if (effBg.a > 0u) {
                DrawBevelFrame(canvas, bevelRect, theme.bevel, /*sunken=*/(!p.disabled && (p.pressed || p.active)), thinBevel);
            }
            if (p.focused && !p.disabled && bevelPx.Width() > step * 8 && bevelPx.Height() > step * 8) {
                const keel::i32 focusInsetPx = hasCustomBevel
                    ? keel::max(step * 3, pg.StrokePx(p.borderWidth) + step)
                    : (step * 3);
                if (bevelPx.Width() > focusInsetPx * 2 + step * 2 && bevelPx.Height() > focusInsetPx * 2 + step * 2) {
                    canvas.DrawRoundRect(
                        pg.PxRectToLogical(
                            bevelPx.x0 + focusInsetPx,
                            bevelPx.y0 + focusInsetPx,
                            bevelPx.x1 - focusInsetPx,
                            bevelPx.y1 - focusInsetPx
                        ),
                        0.0f, Color::Transparent(), theme.palette.focusRing, stepDp
                    );
                }
            }
            break;

        case FrameKind::ToolbarButton:
            if (!p.disabled && (p.pressed || p.active)) {
                DrawBevelFrame(canvas, bevelRect, theme.bevel, /*sunken=*/true, /*thin=*/true);
            } else if (!p.disabled && p.hovered) {
                DrawBevelFrame(canvas, bevelRect, theme.bevel, /*sunken=*/false, /*thin=*/true);
            }
            if (p.focused && !p.disabled && bevelPx.Width() > step * 6 && bevelPx.Height() > step * 6) {
                canvas.DrawRoundRect(
                    pg.PxRectToLogical(
                        bevelPx.x0 + step * 2,
                        bevelPx.y0 + step * 2,
                        bevelPx.x1 - step * 2,
                        bevelPx.y1 - step * 2
                    ),
                    0.0f, Color::Transparent(), theme.palette.focusRing, stepDp
                );
            }
            break;

        case FrameKind::Badge:
            DrawBevelFrame(canvas, bevelRect, theme.bevel, /*sunken=*/true, /*thin=*/true);
            break;

        case FrameKind::Keycap:
            DrawBevelFrame(canvas, bevelRect, theme.bevel, /*sunken=*/(!p.disabled && p.pressed), /*thin=*/true);
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
        m.recessedRegions = true;
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
        const PixelGrid pg = PixelGrid::From(canvas);
        const PixelGrid::SnappedPxRect fp = pg.SnapRectPx(fieldRect);
        const keel::i32 step = pg.UnitStepPx();
        const keel::i32 insetPx = useRaised ? (step * 2) : step;
        const keel::i32 availHPx = keel::max(step * 4, fp.Height() - insetPx * 2);
        const keel::f32 rawBtnW = keel::max(18.0f, keel::min(pg.PxToDp(availHPx), paddingX + iconSize + 4.0f));
        const keel::i32 btnWPx = keel::max(step * 4, pg.StrokePx(rawBtnW));
        const keel::i32 bx1 = fp.x1 - insetPx;
        const keel::i32 bx0 = keel::max(fp.x0 + insetPx, bx1 - btnWPx);
        const Rect dropBtnRect = pg.PxRectToLogical(bx0, fp.y0 + insetPx, bx1, fp.y0 + insetPx + availHPx);
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
        outIconColor = disabled ? pal.textDisabled : ResolveContrastInk(btnBg, pal.textPrimary, theme.bevel);
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
        const PixelGrid pg = PixelGrid::From(canvas);
        const Rect sr = pg.SnapRect(popupRect);
        DrawShadowForRect(canvas, theme, sr, 0.0f, Elevation::Md, 1.0f);
        canvas.DrawRoundRect(sr, 0.0f, pal.inputBackground, theme.bevel.shadowOuter, pg.UnitStepDp());
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
    const SliderTickStyle tickStyle = theme.sliderStyle.tickStyle;
    if (tickStyle == SliderTickStyle::Line) {
        const PixelGrid pg = PixelGrid::From(canvas);
        const keel::i32 step = pg.UnitStepPx();
        const keel::i32 lenPx = step * 3;
        const Color tickCol = disabled ? theme.palette.textDisabled : theme.palette.border;
        const keel::f32 tickOffset = keel::round(keel::max(6.0f, thumbSize * 0.75f));
        if (vertical) {
            const keel::i32 tx = pg.SnapXToPx(crossCoord + tickOffset);
            const keel::i32 ty = pg.SnapYToPx(tickCenter.y);
            canvas.DrawRect(pg.PxRectToLogical(tx, ty, tx + lenPx, ty + step), tickCol);
        } else {
            const keel::i32 tx = pg.SnapXToPx(tickCenter.x);
            const keel::i32 ty = pg.SnapYToPx(crossCoord + tickOffset);
            canvas.DrawRect(pg.PxRectToLogical(tx, ty, tx + step, ty + lenPx), tickCol);
        }
        return;
    }
    if (tickStyle == SliderTickStyle::Notch) {
        const PixelGrid pg = PixelGrid::From(canvas);
        const keel::i32 notchPx = pg.StrokePx(1.5f);
        const keel::f32 notchDp = pg.PxToDp(notchPx);
        const keel::f32 span = keel::max(tickSize + 4.0f, 8.0f);
        const keel::f32 halfSpan = span * 0.5f;
        const Color notchCol = disabled ? color.WithAlphaF(theme.disabledOpacity) : color;
        if (vertical) {
            const keel::f32 y = pg.SnapLineCenterY(tickCenter.y, notchPx);
            canvas.DrawLine(Vec2(crossCoord - halfSpan, y), Vec2(crossCoord + halfSpan, y), notchCol, notchDp);
        } else {
            const keel::f32 x = pg.SnapLineCenterX(tickCenter.x, notchPx);
            canvas.DrawLine(Vec2(x, crossCoord - halfSpan), Vec2(x, crossCoord + halfSpan), notchCol, notchDp);
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
            DrawBevelDividerH(canvas, keel::min(p0.x, p1.x), keel::max(p0.x, p1.x), p0.y, theme.bevel);
        } else {
            DrawBevelDividerV(canvas, p0.x, keel::min(p0.y, p1.y), keel::max(p0.y, p1.y), theme.bevel);
        }
        return;
    }
    if (fallbackColor.a == 0u || thickness <= 0.0f) return;
    const keel::f32 effThick = (theme.chrome.divider == DividerStyle::Bold) ? keel::max(thickness, 2.0f) : thickness;
    PixelGrid::From(canvas).DrawAlignedLine(canvas, p0, p1, fallbackColor, effThick);
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
    keel::f32 /*cornerRadius*/,
    bool explicitColor
) const {
    const auto& pal = theme.palette;
    const bool isBevel = (theme.chrome.frame == FrameTreatment::Bevel3D);
    const bool isBrutal = (theme.chrome.frame == FrameTreatment::HardShadow);
    const bool sharp = isBevel || (theme.sliderStyle.trackCaps == SliderTrackCaps::Square);
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
        canvas.DrawRoundRect(trackRect, trackR, trackBg, bdr, bw);
    } else {
        canvas.DrawRoundRect(trackRect, trackR, trackBg, trackBorder, 1.0f);
    }

    if (hasFill && (theme.sliderStyle.trackStyle == SliderTrackStyle::Filled || explicitColor)) {
        if (isBevel) {
            const Rect insetFill(
                fillRect.x + (vertical ? 1.5f : 1.0f),
                fillRect.y + (vertical ? 0.0f : 1.5f),
                keel::max(1.0f, fillRect.w - (vertical ? 3.0f : 1.0f)),
                keel::max(1.0f, fillRect.h - (vertical ? 1.0f : 3.0f))
            );
            canvas.DrawRect(insetFill, fillColor);
        } else if (theme.fillStyle == FillStyle::Gradient) {
            const Gradient fillGrad = theme.DeriveAccentGradient(fillColor, vertical ? 0.0f : 90.0f);
            canvas.DrawGradientRoundRect(fillRect, trackR, fillGrad);
        } else if (sharp) {
            canvas.DrawRect(fillRect, fillColor);
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
    const SliderThumbShape shape = theme.sliderStyle.thumbShape;

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

    if (theme.chrome.frame == FrameTreatment::Glass && !sharp) {
        const bool isDark = (theme.mode == ThemeMode::Dark);
        if (!p.disabled && theme.shadowsEnabled) {
            DrawShadowForRect(canvas, theme, thumbRect, halfThumb, Elevation::Sm, 0.45f);
        }
        GlassParams gp{};
        gp.tint = thumbBg.WithAlphaF(p.disabled ? theme.disabledOpacity : (p.active ? 0.55f : (isDark ? 0.34f : 0.52f)));
        gp.blurRadius = 6.0f;
        gp.frost = 0.25f;
        gp.refraction = p.disabled ? 0.15f : 0.58f;
        gp.dispersion = p.disabled ? 0.05f : 0.28f;
        gp.rimLight = p.disabled ? 0.25f : 0.78f;
        gp.saturation = 1.30f;
        gp.borderWidth = (p.focused || p.active) ? 1.75f : 1.0f;
        gp.borderColor = (p.focused || p.active || p.hovered)
            ? thumbBorder
            : Color::Lerp(pal.border, theme.bevel.highlightOuter, 0.55f).WithAlphaF(0.75f);
        gp.rimColor = theme.bevel.highlightOuter;
        canvas.DrawGlassRect(thumbRect, halfThumb, gp);
        return;
    }

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
    const PixelGrid pg = PixelGrid::From(canvas);
    const PixelGrid::SnappedPxRect trackPx = pg.SnapRectPx(trackRect);
    const Rect snappedTrack = pg.PxRectToLogical(trackPx);
    const keel::i32 step = pg.UnitStepPx();
    const bool isSeg3D = (theme.chrome.progressFill == ProgressFillStyle::Segmented);
    const bool sharp = isSeg3D || (theme.chrome.frame == FrameTreatment::Bevel3D) || (cornerRadius <= 0.01f);
    const keel::f32 barDim = vertical ? snappedTrack.w : snappedTrack.h;

    if (isSeg3D) {
        const Color bg = disabled ? theme.palette.background.WithAlphaF(theme.disabledOpacity) : theme.palette.background;
        canvas.DrawRect(snappedTrack, bg);
        DrawBevelFrame(canvas, snappedTrack, theme.bevel, /*sunken=*/true, /*thin=*/false);

        const keel::i32 padPx = step * 2;
        if (trackPx.Width() < padPx * 2 + step * 2 || trackPx.Height() < padPx * 2 + step * 2) return;
        const Rect inner = pg.PxRectToLogical(
            trackPx.x0 + padPx,
            trackPx.y0 + padPx,
            trackPx.x1 - padPx,
            trackPx.y1 - padPx
        );

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
                    canvas.DrawRect(pg.SnapRect(Rect(inner.x, inner.Bottom() - pos - blockLen, inner.w, blockLen)), fillColor);
                } else {
                    canvas.DrawRect(pg.SnapRect(Rect(inner.x + pos, inner.y, blockLen, inner.h)), fillColor);
                }
            }
        } else {
            const keel::f32 axisLen = vertical ? inner.h : inner.w;
            const keel::f32 fillLen = ratio * axisLen;
            for (keel::f32 p = 0.0f; p < fillLen - 0.5f; p += stride) {
                const keel::f32 seg = keel::min(blockLen, fillLen - p);
                if (seg >= 2.0f) {
                    if (vertical) {
                        canvas.DrawRect(pg.SnapRect(Rect(inner.x, inner.Bottom() - p - seg, inner.w, seg)), fillColor);
                    } else {
                        canvas.DrawRect(pg.SnapRect(Rect(inner.x + p, inner.y, seg, inner.h)), fillColor);
                    }
                }
            }
        }
        canvas.PopClip();
        return;
    }

    // Modern smooth or sharp bar
    const keel::f32 r = sharp ? 0.0f : (barDim * 0.5f);
    const bool useGradFill = (theme.fillStyle == FillStyle::Gradient) && !disabled;
    auto drawBarFill = [&](Rect fillR) {
        if (useGradFill) {
            const Gradient fillGrad = theme.DeriveAccentGradient(fillColor, vertical ? 0.0f : 90.0f);
            canvas.DrawGradientRoundRect(fillR, r, fillGrad);
        } else {
            canvas.DrawRoundRect(fillR, r, fillColor);
        }
    };
    canvas.DrawRoundRect(snappedTrack, r, trackBg, trackBorder, pg.UnitStepDp());
    if (indeterminate) {
        const keel::f32 pingPong = 1.0f - keel::abs(keel::fmod(animPhase * 2.0f, 2.0f) - 1.0f);
        const keel::f32 smoothT = pingPong * pingPong * (3.0f - 2.0f * pingPong);
        if (vertical) {
            const keel::f32 shuttleH = keel::max(snappedTrack.w, snappedTrack.h * 0.32f);
            const keel::f32 shuttleY = snappedTrack.y + (1.0f - smoothT) * keel::max(0.0f, snappedTrack.h - shuttleH);
            drawBarFill(Rect(snappedTrack.x, shuttleY, snappedTrack.w, shuttleH));
        } else {
            const keel::f32 shuttleW = keel::max(snappedTrack.h, snappedTrack.w * 0.32f);
            const keel::f32 shuttleX = snappedTrack.x + smoothT * keel::max(0.0f, snappedTrack.w - shuttleW);
            drawBarFill(Rect(shuttleX, snappedTrack.y, shuttleW, snappedTrack.h));
        }
    } else if (ratio > 0.001f) {
        if (vertical) {
            const keel::f32 fillH = ratio * snappedTrack.h;
            if (fillH > 0.5f) {
                drawBarFill(Rect(snappedTrack.x, snappedTrack.Bottom() - fillH, snappedTrack.w, fillH));
            }
        } else {
            const keel::f32 fillW = ratio * snappedTrack.w;
            if (fillW > 0.5f) {
                drawBarFill(Rect(snappedTrack.x, snappedTrack.y, fillW, snappedTrack.h));
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
    const PixelGrid pg = PixelGrid::From(canvas);
    const bool isBevel = (theme.chrome.frame == FrameTreatment::Bevel3D);
    const bool sharp = isBevel || (themeRadius <= 0.01f);
    const PixelGrid::SnappedPxRect trackPx = pg.SnapRectPx(trackRect);
    const Rect snappedTrack = isBevel ? pg.PxRectToLogical(trackPx) : trackRect;

    if (isBevel) {
        canvas.DrawRect(snappedTrack, trackFill);
        DrawBevelFrame(canvas, snappedTrack, theme.bevel, /*sunken=*/true, /*thin=*/false);
        if (focused && !disabled) {
            canvas.DrawRoundRect(snappedTrack, 0.0f, Color::Transparent(), theme.palette.focusRing, pg.UnitStepDp());
        }
    } else if (checked && theme.fillStyle == FillStyle::Gradient && !disabled) {
        const Gradient trackGrad = theme.DeriveAccentGradient(trackFill, 90.0f);
        canvas.DrawGradientRoundRect(snappedTrack, sharp ? 0.0f : (snappedTrack.h * 0.5f), trackGrad, trackStroke, pg.StrokeDp(strokeWidth));
    } else {
        canvas.DrawRoundRect(snappedTrack, sharp ? 0.0f : (snappedTrack.h * 0.5f), trackFill, trackStroke, pg.StrokeDp(strokeWidth));
    }

    if (isBevel) {
        const keel::i32 padPx = pg.UnitStepPx() * 2;
        const keel::i32 knobHPx = keel::max(pg.UnitStepPx() * 6, trackPx.Height() - padPx * 2);
        const keel::i32 knobWPx = keel::max(pg.StrokePx(10.0f), pg.StrokePx(knobDiameter));
        const keel::i32 ky0 = trackPx.y0 + (trackPx.Height() - knobHPx) / 2;
        const keel::i32 kx0 = checked ? (trackPx.x1 - knobWPx - padPx) : (trackPx.x0 + padPx);
        const Rect knobRect = pg.PxRectToLogical(kx0, ky0, kx0 + knobWPx, ky0 + knobHPx);
        const Color bevelKnobBg = disabled ? theme.palette.surface.WithAlphaF(theme.disabledOpacity) : theme.palette.surface;
        canvas.DrawRect(knobRect, bevelKnobBg);
        DrawBevelFrame(canvas, knobRect, theme.bevel, /*sunken=*/false, /*thin=*/false);
    } else {
        const keel::f32 knobH = knobDiameter;
        const keel::f32 knobW = knobDiameter;
        const keel::f32 knobY = snappedTrack.y + (snappedTrack.h - knobH) * 0.5f;
        const keel::f32 knobX = checked
            ? (snappedTrack.x + snappedTrack.w - knobW - knobPadding)
            : (snappedTrack.x + knobPadding);
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
    const PixelGrid pg = PixelGrid::From(canvas);
    const Rect sr = pg.SnapRect(boxRect);
    if (theme.chrome.checkControl == CheckControlStyle::SunkenInk) {
        const auto& pal = theme.palette;
        const Color bevelFill = (disabled || pressed) ? pal.surface : pal.inputBackground;
        canvas.DrawRect(sr, bevelFill);
        DrawBevelFrame(canvas, sr, theme.bevel, /*sunken=*/true, /*thin=*/false);
        if (focused && !disabled) {
            canvas.DrawRoundRect(sr, 0.0f, Color::Transparent(), pal.focusRing, pg.UnitStepDp());
        }
        if (disabled) return pal.textDisabled;
        return ResolveContrastInk(bevelFill, pal.textPrimary, theme.bevel);
    }

    canvas.DrawRoundRect(sr, cornerRadius, fill, stroke, pg.StrokeDp(strokeWidth));
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
        if (disabled) return pal.textDisabled;
        return ResolveContrastInk(bevelFill, pal.textPrimary, theme.bevel);
    }

    canvas.DrawRoundRect(circleRect(center, radius), radius, fill, stroke, strokeWidth);
    return defaultDotColor;
}

void DefaultStylePainter::DrawTabBarBaseline(
    ICanvas& canvas,
    const UiTheme& theme,
    Rect barRect
) const {
    const PixelGrid pg = PixelGrid::From(canvas);
    const PixelGrid::SnappedPxRect bp = pg.SnapRectPx(barRect);
    const keel::i32 step = pg.UnitStepPx();
    if (bp.Width() <= 0 || bp.Height() <= 0) return;

    if (theme.chrome.tabChrome == TabBarChrome::Classic3D) {
        canvas.DrawRect(pg.PxRectToLogical(bp.x0, bp.y1 - step, bp.x1, bp.y1), theme.bevel.highlightOuter);
        return;
    }
    const keel::f32 stepDp = pg.PxToDp(step);
    const keel::f32 sepY = pg.PxToLogicalY(bp.y1) - stepDp * 0.5f;
    canvas.DrawLine(
        Vec2(pg.PxToLogicalX(bp.x0), sepY),
        Vec2(pg.PxToLogicalX(bp.x1), sepY),
        theme.palette.borderSubtle,
        stepDp
    );
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
    const PixelGrid pg = PixelGrid::From(canvas);
    const PixelGrid::SnappedPxRect bp = pg.SnapRectPx(bounds);
    const keel::i32 step = pg.UnitStepPx();
    const keel::f32 stepDp = pg.PxToDp(step);

    if (theme.chrome.tabChrome == TabBarChrome::Classic3D && style != TabChromeStyle::Folder) {
        const BevelPalette& bc = theme.bevel;
        const keel::i32 s2 = step * 2;
        const keel::i32 topOffPx = active ? 0 : keel::max(step * 3, pg.StrokePx(3.0f));
        const keel::i32 x0 = bp.x0;
        const keel::i32 y0 = bp.y0 + topOffPx;
        const keel::i32 x1 = bp.x1;
        const keel::i32 y1 = bp.y1;
        const keel::i32 wPx = x1 - x0;
        const keel::i32 hPx = keel::max(step * 4, y1 - y0);
        if (wPx < step * 4) return active ? -0.5f : 1.5f;
        const keel::i32 effY0 = y1 - hPx;

        const Color tabBg = active
            ? pal.surface
            : ((hovered && !disabled) ? pal.surfaceHover : pal.background);

        if (wPx > step * 4 && hPx > s2) {
            canvas.DrawRect(pg.PxRectToLogical(x0 + s2, effY0, x1 - s2, effY0 + step), tabBg);
            canvas.DrawRect(pg.PxRectToLogical(x0 + step, effY0 + step, x1 - step, effY0 + s2), tabBg);
            canvas.DrawRect(pg.PxRectToLogical(x0, effY0 + s2, x1, y1), tabBg);
        } else {
            canvas.DrawRect(pg.PxRectToLogical(x0, effY0, x1, y1), tabBg);
        }

        // Top & left raised bevel with 2-step chamfered top-left corner
        canvas.DrawRect(pg.PxRectToLogical(x0 + s2, effY0, x1 - s2, effY0 + step), bc.highlightOuter);
        canvas.DrawRect(pg.PxRectToLogical(x0 + step, effY0 + step, x0 + s2, effY0 + s2), bc.highlightOuter);
        canvas.DrawRect(pg.PxRectToLogical(x0, effY0 + s2, x0 + step, y1), bc.highlightOuter);

        canvas.DrawRect(pg.PxRectToLogical(x0 + s2, effY0 + step, x1 - s2, effY0 + s2), bc.highlightInner);
        canvas.DrawRect(pg.PxRectToLogical(x0 + step, effY0 + s2, x0 + s2, y1), bc.highlightInner);

        // Right raised bevel with 2-step chamfered top-right corner
        canvas.DrawRect(pg.PxRectToLogical(x1 - s2, effY0 + step, x1 - step, effY0 + s2), bc.shadowOuter);
        canvas.DrawRect(pg.PxRectToLogical(x1 - step, effY0 + s2, x1, y1), bc.shadowOuter);
        canvas.DrawRect(pg.PxRectToLogical(x1 - s2, effY0 + s2, x1 - step, y1), bc.shadowInner);

        if (active) {
            canvas.DrawRect(pg.PxRectToLogical(x0 + s2, y1 - step, x1 - s2, y1), tabBg);
        } else if (showSeparator) {
            canvas.DrawRect(pg.PxRectToLogical(x0, y1 - step, x1, y1), bc.highlightOuter);
        }

        if (focused && !disabled && wPx > step * 8 && hPx > step * 8) {
            canvas.DrawRoundRect(
                pg.PxRectToLogical(x0 + step * 3, effY0 + step * 3, x1 - step * 3, y1 - s2),
                0.0f, Color::Transparent(), pal.focusRing, stepDp
            );
        }
        return active ? -0.5f : 1.5f;
    }

    // Modern TabBar chrome
    const Rect snappedBounds = pg.PxRectToLogical(bp);
    const keel::f32 baseSepY = pg.PxToLogicalY(bp.y1) - stepDp * 0.5f;
    if (style == TabChromeStyle::Underline) {
        if (showSeparator) {
            canvas.DrawLine(Vec2(snappedBounds.x, baseSepY), Vec2(snappedBounds.Right(), baseSepY), pal.borderSubtle, stepDp);
        }
        if (hovered && !active && !disabled) {
            const Rect hoverR(snappedBounds.x, snappedBounds.y + 4.0f, snappedBounds.w, snappedBounds.h - 8.0f);
            canvas.DrawRoundRect(hoverR, radius, pal.surfaceHover);
        }
        if (active) {
            const keel::f32 lineH = 3.0f;
            const keel::f32 linePad = 2.0f;
            const Rect lineR(snappedBounds.x + linePad, snappedBounds.Bottom() - lineH, keel::max(0.0f, snappedBounds.w - linePad * 2.0f), lineH);
            canvas.DrawRoundRect(lineR, radius > 0.01f ? 1.5f : 0.0f, accentCol);
        }
        if (focused && !disabled) {
            const keel::f32 bottomInset = active ? 6.0f : 4.0f;
            canvas.DrawRoundRect(Rect(snappedBounds.x + 1.0f, snappedBounds.y + 2.0f, keel::max(0.0f, snappedBounds.w - 2.0f), keel::max(0.0f, snappedBounds.h - bottomInset)), radius, Color::Transparent(), pal.focusRing, pg.StrokeDp(1.5f));
        }
    } else if (style == TabChromeStyle::Card) {
        const Rect cardR = pg.SnapRect(Rect(snappedBounds.x, snappedBounds.y + 2.0f, snappedBounds.w, snappedBounds.h + radius));
        if (active) {
            canvas.DrawRoundRect(cardR, radius, pal.surface, pal.border, stepDp);
            canvas.DrawRect(pg.PxRectToLogical(bp.x0 + step, bp.y1 - step * 2, bp.x1 - step, bp.y1), pal.surface);
            canvas.DrawRoundRect(Rect(cardR.x + stepDp, cardR.y, keel::max(0.0f, cardR.w - stepDp * 2.0f), 2.5f), radius > 0.01f ? 1.0f : 0.0f, accentCol);
        } else {
            if (showSeparator) {
                canvas.DrawLine(Vec2(snappedBounds.x, baseSepY), Vec2(snappedBounds.Right(), baseSepY), pal.borderSubtle, stepDp);
            }
            if (hovered && !disabled) {
                canvas.DrawRoundRect(cardR, radius, pal.surfaceHover, pal.borderSubtle, stepDp);
            }
        }
        if (focused && !disabled) {
            const Rect focusCardR(cardR.x + 0.75f, cardR.y + 0.75f, keel::max(0.0f, cardR.w - 1.5f), cardR.h);
            canvas.DrawRoundRect(focusCardR, keel::max(0.0f, radius - 0.5f), Color::Transparent(), pal.focusRing, pg.StrokeDp(1.5f));
        }
    } else if (style == TabChromeStyle::Folder) {
        const keel::f32 slope = keel::min(6.0f, snappedBounds.w * 0.14f);
        const keel::f32 topY = snappedBounds.y + (active ? 1.5f : 4.0f);
        const keel::f32 botY = snappedBounds.Bottom();
        VectorPath folderPath;
        folderPath.MoveTo({snappedBounds.x, botY});
        folderPath.LineTo({snappedBounds.x + slope, topY});
        folderPath.LineTo({snappedBounds.Right() - slope, topY});
        folderPath.LineTo({snappedBounds.Right(), botY});
        folderPath.Close();

        if (active || (hovered && !disabled)) {
            PathPaint p{};
            p.fillColor = active ? pal.surfaceElevated : pal.surfaceHover;
            p.renderFill = true;
            p.strokeColor = active ? pal.border : pal.borderSubtle;
            p.strokeWidthPx = stepDp;
            p.renderStroke = true;
            canvas.DrawPath(folderPath, p);
        }
        if (active) {
            canvas.DrawRect(pg.PxRectToLogical(bp.x0 + step, bp.y1 - step * 2, bp.x1 - step, bp.y1), pal.surfaceElevated);
            canvas.DrawLine(Vec2(snappedBounds.x + slope, topY + 0.5f), Vec2(snappedBounds.Right() - slope, topY + 0.5f), accentCol, pg.StrokeDp(2.0f));
        } else if (showSeparator) {
            canvas.DrawLine(Vec2(snappedBounds.x, baseSepY), Vec2(snappedBounds.Right(), baseSepY), pal.borderSubtle, stepDp);
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
            const bool isGradPill = (theme.fillStyle == FillStyle::Gradient) && !disabled;
            if (theme.chrome.frame == FrameTreatment::Glass) {
                GlassParams gp{};
                if (isGradPill) {
                    const Gradient g = theme.DeriveAccentGradient(pillActiveCol, 135.0f);
                    gp.tint = g.stops[0].color.WithAlphaF(0.76f);
                    gp.tintEnd = g.stops[1].color.WithAlphaF(0.76f);
                    gp.tintAngleDeg = 135.0f;
                } else {
                    gp.tint = pillActiveCol.WithAlphaF(0.76f);
                }
                gp.blurRadius = 10.0f;
                gp.frost = 0.65f;
                gp.refraction = 0.35f;
                gp.dispersion = 0.18f;
                gp.rimLight = 0.58f;
                gp.saturation = 1.24f;
                gp.borderWidth = (focusBorderW > 0.0f) ? focusBorderW : 1.0f;
                gp.borderColor = (focusBorderW > 0.0f)
                    ? focusBorder
                    : theme.bevel.highlightOuter.WithAlphaF(theme.mode == ThemeMode::Dark ? 0.35f : 0.55f);
                gp.rimColor = theme.bevel.highlightOuter;
                canvas.DrawGlassRect(bounds, radius, gp);
            } else if (isGradPill) {
                const Gradient g = theme.DeriveAccentGradient(pillActiveCol, 135.0f);
                canvas.DrawGradientRoundRect(bounds, radius, g, focusBorder, focusBorderW);
            } else {
                canvas.DrawRoundRect(bounds, radius, pillActiveCol, focusBorder, focusBorderW);
            }
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
        const Rect bevelRect = PixelGrid::From(canvas).SnapRect(rect);
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
    if (theme.fillStyle == FillStyle::Gradient && bg.a >= 180u && EstimateColorChroma(bg) >= 0.22f) {
        const Gradient badgeGrad = theme.DeriveAccentGradient(bg, 135.0f);
        canvas.DrawGradientRoundRect(rect, r, badgeGrad, border, borderWidth);
    } else {
        canvas.DrawRoundRect(rect, r, bg, border, borderWidth);
    }
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

    const PixelGrid pg = PixelGrid::From(canvas);
    const PixelGrid::SnappedPxRect sp = pg.SnapRectPx(sashRect);
    const Rect snappedSash = pg.PxRectToLogical(sp);
    const keel::i32 step = pg.UnitStepPx();

    const auto& pal = theme.palette;
    const bool active = !disabled && (hovered || dragging);
    const keel::f32 thick = verticalBar ? snappedSash.w : snappedSash.h;
    const keel::f32 span = verticalBar ? snappedSash.h : snappedSash.w;

    // 1. Classic 3D (Bevel3D / Etched3D): raised 3D splitter bar + embossed 3D grip dots
    if (theme.chrome.frame == FrameTreatment::Bevel3D || theme.chrome.divider == DividerStyle::Etched3D) {
        const Color baseBg = disabled
            ? pal.surface
            : (dragging ? pal.surfaceActive : (hovered ? pal.surfaceHover : pal.surface));
        canvas.DrawRect(snappedSash, baseBg);

        if (thick < 4.0f) {
            if (verticalBar) {
                const keel::f32 x = pg.PxToLogicalX((sp.x0 + sp.x1) / 2 - step);
                DrawBevelDividerV(canvas, x, snappedSash.y, snappedSash.Bottom(), theme.bevel);
            } else {
                const keel::f32 y = pg.PxToLogicalY((sp.y0 + sp.y1) / 2 - step);
                DrawBevelDividerH(canvas, snappedSash.x, snappedSash.Right(), y, theme.bevel);
            }
        } else {
            const bool thinBevel = (thick < 8.0f);
            DrawBevelFrame(canvas, snappedSash, theme.bevel, /*sunken=*/(dragging && !disabled), thinBevel);

            if (showGrip && span >= 24.0f) {
                const keel::i32 cxPx = sp.x0 + sp.Width() / 2;
                const keel::i32 cyPx = sp.y0 + sp.Height() / 2;
                const keel::i32 pitchPx = keel::max(step * 4, pg.StrokePx(4.0f));
                const Color dotShadow = disabled
                    ? theme.bevel.shadowInner.WithAlphaF(theme.disabledOpacity)
                    : theme.bevel.shadowInner;
                const Color dotHi = disabled
                    ? theme.bevel.highlightOuter.WithAlphaF(theme.disabledOpacity)
                    : theme.bevel.highlightOuter;

                for (keel::i32 i = -2; i <= 2; ++i) {
                    const keel::i32 offPx = i * pitchPx;
                    if (verticalBar) {
                        const keel::i32 dy = cyPx + offPx;
                        canvas.DrawRect(pg.PxRectToLogical(cxPx - step, dy - step, cxPx, dy), dotShadow);
                        canvas.DrawRect(pg.PxRectToLogical(cxPx, dy, cxPx + step, dy + step), dotHi);
                    } else {
                        const keel::i32 dx = cxPx + offPx;
                        canvas.DrawRect(pg.PxRectToLogical(dx - step, cyPx - step, dx, cyPx), dotShadow);
                        canvas.DrawRect(pg.PxRectToLogical(dx, cyPx, dx + step, cyPx + step), dotHi);
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

    const keel::f32 lineT = isBrutalist ? keel::min(2.0f, thick) : pg.PxToDp(step);
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

void DefaultStylePainter::DrawPanelToggle(
    ICanvas& canvas,
    const UiTheme& theme,
    const PanelTogglePaintParams& p
) const {
    if (p.rect.w <= 0.0f || p.rect.h <= 0.0f) return;

    const auto& pal = theme.palette;
    const bool isBevel = (theme.chrome.frame == FrameTreatment::Bevel3D);
    const bool isBrutal = (theme.chrome.frame == FrameTreatment::HardShadow);
    const keel::f32 s = keel::max(0.5f, p.scale);
    const keel::f32 alphaMul = (isBevel || isBrutal || p.highlighted) ? 1.0f : 0.80f;

    const Color baseBg = isBevel
        ? (p.pressed ? pal.surfaceActive : (p.hovered ? pal.surfaceHover : pal.surface))
        : (p.pressed ? pal.surfaceActive : (p.hovered ? pal.surfaceHover : pal.surfaceElevated));
    const Color bg = baseBg.WithAlphaF(alphaMul);
    const Color border = ((p.hovered || p.pressed) ? pal.accent : pal.border).WithAlphaF(alphaMul);
    const keel::f32 radius = (isBevel || isBrutal)
        ? 0.0f
        : keel::min(5.0f * s, theme.GetRadius(UiSize::Sm));

    DrawFrame(canvas, theme, FrameKind::Button, FramePaintParams{
        .rect = p.rect,
        .radius = radius,
        .explicitRadius = false,
        .bg = bg,
        .border = border,
        .borderWidth = 1.0f * s,
        .hovered = p.hovered,
        .pressed = p.pressed,
        .accent = pal.accent
    });

    const bool horizontal = (p.direction == PanelToggleDirection::Left || p.direction == PanelToggleDirection::Right);
    const bool isStart = (p.direction == PanelToggleDirection::Up || p.direction == PanelToggleDirection::Left);
    const ScrollbarArrowStyle arrowStyle = (isBevel || theme.scrollbarStyle.arrowStyle == ScrollbarArrowStyle::Triangle)
        ? ScrollbarArrowStyle::Triangle
        : ScrollbarArrowStyle::Chevron;
    const Color arrowCol = isBevel
        ? pal.textPrimary
        : ((p.hovered || p.pressed) ? pal.accent : pal.textSecondary).WithAlphaF(alphaMul);
    const keel::f32 shift = GetButtonContentOffset(theme, p.pressed, false, false);
    const Rect glyphRect = isBevel ? PixelGrid::From(canvas).SnapRect(p.rect) : p.rect;
    DrawScrollbarArrowGlyph(canvas, glyphRect, horizontal, isStart, arrowStyle, arrowCol, shift);
}

void DefaultStylePainter::DrawTableHeaderCell(
    ICanvas& canvas,
    const UiTheme& theme,
    const TableHeaderCellPaintParams& p
) const {
    if (p.rect.w <= 0.0f || p.rect.h <= 0.0f) return;
    const auto& pal = theme.palette;
    const PixelGrid pg = PixelGrid::From(canvas);
    const Rect sr = pg.SnapRect(p.rect);

    if (theme.chrome.frame == FrameTreatment::Bevel3D) {
        const Color faceCol = p.pressed ? pal.surfaceActive : pal.surface;
        canvas.DrawRect(sr, faceCol);
        DrawBevelFrame(canvas, sr, theme.bevel, p.pressed, false);
        if (p.dividerActive && p.showRightDivider) {
            const keel::i32 divPx = pg.StrokePx(2.0f);
            const keel::f32 divX = pg.SnapLineCenterX(sr.Right() - pg.PxToDp(divPx) * 0.5f, divPx);
            canvas.DrawLine(Vec2(divX, sr.y), Vec2(divX, sr.Bottom()), pal.accent, pg.PxToDp(divPx));
        }
        return;
    }

    const bool isBold =
        (theme.chrome.frame == FrameTreatment::HardShadow || theme.chrome.divider == DividerStyle::Bold);
    const Color fillCol = p.hovered
        ? pal.surfaceHover
        : (p.sorted ? pal.accent.WithAlpha(isBold ? 28 : 20) : pal.surfaceElevated);
    canvas.DrawRect(sr, fillCol);

    if (p.showRightDivider) {
        const Color divCol = p.dividerActive
            ? pal.accent
            : (p.isFrozenSeam ? pal.borderHover : pal.border);
        const keel::i32 divPx = pg.StrokePx(p.dividerActive ? 2.0f : (isBold ? 1.5f : 1.0f));
        const keel::f32 divThick = pg.PxToDp(divPx);
        const keel::f32 divX = pg.SnapLineCenterX(sr.Right() - divThick * 0.5f, divPx);
        const keel::f32 padY = (p.isFrozenSeam || isBold) ? 0.0f : 4.0f;
        canvas.DrawLine(
            Vec2(divX, sr.y + padY),
            Vec2(divX, sr.Bottom() - padY),
            divCol,
            divThick
        );
    }
}

void DefaultStylePainter::DrawTreeExpander(
    ICanvas& canvas,
    const UiTheme& theme,
    Rect rect,
    bool expanded,
    bool hovered,
    bool /*selected*/,
    bool disabled,
    Color fallbackColor
) const {
    if (rect.w <= 0.0f || rect.h <= 0.0f) return;

    if (theme.chrome.frame == FrameTreatment::Bevel3D) {
        const auto& pal = theme.palette;
        const PixelGrid pg = PixelGrid::From(canvas);
        const keel::i32 step = pg.UnitStepPx();
        const keel::i32 minPx = keel::as<keel::i32>(keel::floor(keel::min(rect.w, rect.h) * pg.toPx.scale));
        // Authentic Windows 98 SysTreeView32 odd-sized pixel-snapped [+] / [-] expander box
        keel::i32 boxPx = keel::clamp(keel::as<keel::i32>(keel::floor(keel::as<keel::f32>(minPx) * 0.78f)), step * 9, step * 11);
        if ((boxPx & 1) != (step & 1)) {
            boxPx -= 1;
        }
        boxPx = keel::max(boxPx, step * 5);

        const keel::i32 cxPx = pg.SnapXToPx(rect.Center().x);
        const keel::i32 cyPx = pg.SnapYToPx(rect.Center().y);
        const keel::i32 x0 = cxPx - (boxPx / 2);
        const keel::i32 y0 = cyPx - (boxPx / 2);
        const keel::i32 x1 = x0 + boxPx;
        const keel::i32 y1 = y0 + boxPx;

        const Color fillCol = disabled ? pal.surface : pal.inputBackground;
        const Color borderCol = disabled
            ? pal.borderSubtle
            : (hovered ? Color::Lerp(pal.border, pal.textPrimary, 0.35f) : pal.border);
        const Color inkCol = disabled
            ? pal.textDisabled
            : ResolveContrastInk(fillCol, pal.textPrimary, theme.bevel);

        canvas.DrawRect(pg.PxRectToLogical(x0, y0, x1, y1), fillCol);
        // Crisp step-px perimeter border
        canvas.DrawRect(pg.PxRectToLogical(x0, y0, x1, y0 + step), borderCol);
        canvas.DrawRect(pg.PxRectToLogical(x0, y1 - step, x1, y1), borderCol);
        canvas.DrawRect(pg.PxRectToLogical(x0, y0, x0 + step, y1), borderCol);
        canvas.DrawRect(pg.PxRectToLogical(x1 - step, y0, x1, y1), borderCol);

        // Centered '-' bar (and vertical '|' bar when collapsed to form '+')
        const keel::i32 margin = step * 2;
        const keel::i32 barSpan = keel::max(step * 3, boxPx - margin * 2);
        const keel::i32 barX0 = x0 + (boxPx - barSpan) / 2;
        const keel::i32 barY0 = y0 + (boxPx - step) / 2;
        canvas.DrawRect(pg.PxRectToLogical(barX0, barY0, barX0 + barSpan, barY0 + step), inkCol);
        if (!expanded) {
            const keel::i32 vBarX0 = x0 + (boxPx - step) / 2;
            const keel::i32 vBarY0 = y0 + (boxPx - barSpan) / 2;
            canvas.DrawRect(pg.PxRectToLogical(vBarX0, vBarY0, vBarX0 + step, vBarY0 + barSpan), inkCol);
        }
        return;
    }

    const Icon chev = expanded ? Icon::ChevronDown() : Icon::ChevronRight();
    chev.Draw(canvas, rect, fallbackColor);
}

namespace {

void DrawWinCtrlMinimizeGlyph(ICanvas& canvas, Rect r, Color col, bool classicBold) noexcept {
    const PixelGrid pg = PixelGrid::From(canvas);
    if (classicBold) {
        const PixelGrid::SnappedPxRect s = pg.SnapRectPx(r);
        const keel::i32 step = pg.UnitStepPx();
        const keel::i32 barW = keel::max(step * 5, keel::as<keel::i32>(keel::round(keel::as<keel::f32>(s.Width()) * 0.72f)));
        const keel::i32 x0 = s.x0 + (s.Width() - barW) / 2;
        const keel::i32 y1 = s.y1;
        canvas.DrawRect(pg.PxRectToLogical(x0, y1 - step * 2, x0 + barW, y1), col);
        return;
    }
    const keel::i32 strokePx = pg.StrokePx(1.25f);
    const keel::f32 y = pg.SnapLineCenterY(r.Center().y, strokePx);
    canvas.DrawLine(Vec2(pg.SnapEdgeX(r.x), y), Vec2(pg.SnapEdgeX(r.Right()), y), col, pg.PxToDp(strokePx));
}

void DrawWinCtrlMaximizeGlyph(ICanvas& canvas, Rect r, Color col, bool classicBold) noexcept {
    const PixelGrid pg = PixelGrid::From(canvas);
    if (classicBold) {
        const PixelGrid::SnappedPxRect s = pg.SnapRectPx(r);
        const keel::i32 step = pg.UnitStepPx();
        const keel::i32 x0 = s.x0;
        const keel::i32 y0 = s.y0;
        const keel::i32 x1 = keel::max(x0 + step * 6, s.x1);
        const keel::i32 y1 = keel::max(y0 + step * 6, s.y1);
        canvas.DrawRect(pg.PxRectToLogical(x0, y0, x1, y0 + step * 2), col);
        canvas.DrawRect(pg.PxRectToLogical(x0, y1 - step, x1, y1), col);
        canvas.DrawRect(pg.PxRectToLogical(x0, y0, x0 + step, y1), col);
        canvas.DrawRect(pg.PxRectToLogical(x1 - step, y0, x1, y1), col);
        return;
    }
    const Rect sq = pg.SnapRect(r);
    canvas.DrawRoundRect(sq, 1.0f, Color::Transparent(), col, pg.StrokeDp(1.25f));
}

void DrawWinCtrlRestoreGlyph(ICanvas& canvas, Rect r, Color col, bool classicBold) noexcept {
    const PixelGrid pg = PixelGrid::From(canvas);
    if (classicBold) {
        const PixelGrid::SnappedPxRect s = pg.SnapRectPx(r);
        const keel::i32 step = pg.UnitStepPx();
        const keel::i32 szPx = keel::max(step * 4, keel::as<keel::i32>(keel::round(keel::as<keel::f32>(s.Width()) * 0.76f)));
        const keel::i32 fx = s.x0;
        const keel::i32 fy = s.y1 - szPx;
        canvas.DrawRect(pg.PxRectToLogical(fx, fy, fx + szPx, fy + step * 2), col);
        canvas.DrawRect(pg.PxRectToLogical(fx, fy + szPx - step, fx + szPx, fy + szPx), col);
        canvas.DrawRect(pg.PxRectToLogical(fx, fy, fx + step, fy + szPx), col);
        canvas.DrawRect(pg.PxRectToLogical(fx + szPx - step, fy, fx + szPx, fy + szPx), col);

        const keel::i32 bx = s.x1 - szPx;
        const keel::i32 by = s.y0;
        canvas.DrawRect(pg.PxRectToLogical(bx, by, bx + szPx, by + step * 2), col);
        canvas.DrawRect(pg.PxRectToLogical(bx + szPx - step, by, bx + szPx, by + szPx), col);
        if (fy > by) {
            canvas.DrawRect(pg.PxRectToLogical(bx, by, bx + step, fy), col);
        }
        if (bx + szPx > fx + szPx) {
            canvas.DrawRect(pg.PxRectToLogical(fx + szPx, by + szPx - step, bx + szPx, by + szPx), col);
        }
        return;
    }
    const keel::f32 sz = keel::max(4.0f, keel::round(r.w * 0.76f));
    const Rect front = pg.SnapRect(Rect(r.x, r.Bottom() - sz, sz, sz));
    const keel::i32 strokePx = pg.StrokePx(1.2f);
    const keel::f32 strokeDp = pg.PxToDp(strokePx);
    canvas.DrawRoundRect(front, 1.0f, Color::Transparent(), col, strokeDp);
    const keel::f32 bx1 = pg.SnapLineCenterX(r.Right() - sz, strokePx);
    const keel::f32 by1 = pg.SnapLineCenterY(r.y, strokePx);
    const keel::f32 bx2 = pg.SnapLineCenterX(r.Right() - strokeDp, strokePx);
    const keel::f32 by2 = pg.SnapLineCenterY(r.y + sz - strokeDp, strokePx);
    canvas.DrawLine(Vec2(bx1, by1), Vec2(bx2, by1), col, strokeDp);
    canvas.DrawLine(Vec2(bx2, by1), Vec2(bx2, by2), col, strokeDp);
    canvas.DrawLine(Vec2(bx1, by1), Vec2(bx1, front.y), col, strokeDp);
    canvas.DrawLine(Vec2(front.Right(), by2), Vec2(bx2, by2), col, strokeDp);
}

void DrawWinCtrlCloseGlyph(ICanvas& canvas, Rect r, Color col, bool classicBold) noexcept {
    const keel::f32 x0 = keel::round(r.x) + 0.5f;
    const keel::f32 y0 = keel::round(r.y) + 0.5f;
    const keel::f32 x1 = keel::round(r.Right()) - 0.5f;
    const keel::f32 y1 = keel::round(r.Bottom()) - 0.5f;
    const keel::f32 stroke = classicBold ? 1.75f : 1.3f;
    canvas.DrawLine(Vec2(x0, y0), Vec2(x1, y1), col, stroke);
    canvas.DrawLine(Vec2(x1, y0), Vec2(x0, y1), col, stroke);
}

} // namespace

HeaderBarChromeMetrics DefaultStylePainter::GetHeaderBarMetrics(
    const UiTheme& theme,
    bool focused
) const noexcept {
    const auto& pal = theme.palette;
    if (theme.chrome.frame == FrameTreatment::Bevel3D) {
        const Color captionBase = focused ? pal.accent : pal.border;
        const Color preferredInk = focused ? pal.accentText : pal.surfaceHover;
        const Color titleInk = theme.EnsureReadableInk(preferredInk, captionBase);
        return HeaderBarChromeMetrics{
            .titleColor = titleInk,
            .subtitleColor = titleInk.WithAlphaF(0.82f),
            .defaultHeightDelta = -6.0f,
            .controlBtnWidth = 20.0f,
            .controlBtnHeight = 18.0f,
            .controlGap = 0.0f,
            .closeExtraGap = 2.0f,
            .rightEdgePadding = 5.0f,
            .accentCaption = true
        };
    }

    return HeaderBarChromeMetrics{
        .titleColor = focused ? pal.textPrimary : pal.textSecondary,
        .subtitleColor = focused ? pal.textSecondary : pal.textMuted,
        .defaultHeightDelta = 0.0f,
        .controlBtnWidth = 38.0f,
        .controlBtnHeight = 0.0f,
        .controlGap = 6.0f,
        .closeExtraGap = 0.0f,
        .rightEdgePadding = 4.0f,
        .accentCaption = false
    };
}

void DefaultStylePainter::DrawHeaderBar(
    ICanvas& canvas,
    const UiTheme& theme,
    const HeaderBarPaintParams& p
) const {
    if (p.rect.w <= 0.5f || p.rect.h <= 0.5f) return;
    const auto& pal = theme.palette;

    if (theme.chrome.frame == FrameTreatment::Bevel3D) {
        const PixelGrid pg = PixelGrid::From(canvas);
        const PixelGrid::SnappedPxRect sp = pg.SnapRectPx(p.rect);
        const Rect sr = pg.PxRectToLogical(sp);
        const keel::i32 step = pg.UnitStepPx();
        // Classic 3D window title bar: outer 2-step 3D face rim + horizontal gradient caption strip inside
        canvas.DrawRect(sr, pal.surface);

        const keel::i32 insetPx = (sp.Width() > step * 10 && sp.Height() > step * 8) ? (step * 2) : 0;
        const Rect bar = pg.PxRectToLogical(
            sp.x0 + insetPx,
            sp.y0 + insetPx,
            keel::max(sp.x0 + insetPx + step, sp.x1 - insetPx),
            keel::max(sp.y0 + insetPx + step, sp.y1 - insetPx)
        );
        if (p.explicitBg && p.bg.a > 0u) {
            canvas.DrawRect(bar, p.bg);
        } else {
            const Color startCol = p.focused ? pal.accent : pal.border;
            keel::f32 ah = 0.0f;
            keel::f32 as = 0.0f;
            keel::f32 al = 0.0f;
            pal.accent.ToHsl(ah, as, al);
            const keel::f32 accentDeltaL = (al > 0.55f) ? -0.14f : 0.19f;
            const Color endCol = p.focused
                ? pal.accent.ShiftHue(-36.0f, -0.14f, accentDeltaL)
                : pal.border.ShiftLightness(theme.mode == ThemeMode::Dark ? 0.14f : 0.22f);
            const Gradient grad = Gradient::LinearPoints(startCol, endCol, Vec2{0.0f, 0.5f}, Vec2{1.0f, 0.5f});
            canvas.DrawGradientRoundRect(bar, 0.0f, grad, Color::Transparent(), 0.0f);
        }

        if (p.drawBottomBorder && p.borderWidth > 0.0f && p.borderColor.a > 0u) {
            DrawBevelDividerH(canvas, sr.x, sr.Right(), pg.PxToLogicalY(sp.y1 - step * 2), theme.bevel);
        }
        return;
    }

    if (p.topRadius > 0.5f) {
        const keel::f32 tr = keel::min(p.topRadius, p.rect.h * 0.5f);
        canvas.PushClip(Rect(p.rect.x, p.rect.y, p.rect.w, tr));
        canvas.DrawRoundRect(p.rect, tr, p.bg);
        canvas.PopClip();
        if (p.rect.h > tr) {
            canvas.DrawRect(Rect(p.rect.x, p.rect.y + tr, p.rect.w, p.rect.h - tr), p.bg);
        }
    } else {
        DrawSurfaceBg(canvas, theme, SurfacePaintParams{
            .rect = p.rect,
            .role = SurfaceRole::Flat,
            .bg = p.bg
        });
    }

    if (p.drawBottomBorder && p.borderWidth > 0.0f && p.borderColor.a > 0u) {
        const keel::f32 y = p.rect.Bottom() - p.borderWidth * 0.5f;
        DrawDivider(canvas, theme, Vec2(p.rect.x, y), Vec2(p.rect.Right(), y), p.borderColor, p.borderWidth);
    }
}

void DefaultStylePainter::DrawWindowControlButton(
    ICanvas& canvas,
    const UiTheme& theme,
    const WindowControlPaintParams& p
) const {
    const Rect& r = p.rect;
    if (r.w <= 1.0f || r.h <= 1.0f) return;
    const auto& pal = theme.palette;

    if (p.macStyle) {
        const keel::f32 d = keel::min(12.0f, keel::min(r.w, r.h) - 4.0f);
        const Rect dot(r.Center().x - d * 0.5f, r.Center().y - d * 0.5f, d, d);
        UiVariant var = UiVariant::Danger;
        if (p.kind == WindowControlKind::Minimize) var = UiVariant::Warning;
        else if (p.kind == WindowControlKind::Maximize) var = UiVariant::Success;

        const Color semantic = theme.GetSemanticAccent(var);
        const Color fill = p.disabled
            ? pal.surfaceHover
            : (p.focused || p.hovered || p.pressed
                ? (p.pressed ? semantic.WithAlphaF(0.80f) : semantic)
                : pal.border);
        canvas.DrawRoundRect(dot, d * 0.5f, fill, pal.borderSubtle.WithAlphaF(0.5f), 0.75f);

        if ((p.hovered || p.pressed) && !p.disabled) {
            const keel::f32 g = d * 0.48f;
            const Rect gr(dot.Center().x - g * 0.5f, dot.Center().y - g * 0.5f, g, g);
            const Color ink = theme.EnsureReadableInk(pal.background, fill);
            if (p.kind == WindowControlKind::Minimize) DrawWinCtrlMinimizeGlyph(canvas, gr, ink, false);
            else if (p.kind == WindowControlKind::Maximize) DrawWinCtrlMaximizeGlyph(canvas, gr, ink, false);
            else DrawWinCtrlCloseGlyph(canvas, gr, ink, false);
        }
        return;
    }

    if (theme.chrome.frame == FrameTreatment::Bevel3D) {
        const Rect sr = PixelGrid::From(canvas).SnapRect(r);
        const Color face = p.disabled
            ? pal.surface
            : (p.pressed ? pal.surfaceActive : (p.hovered ? pal.surfaceHover : pal.surface));
        canvas.DrawRect(sr, face);
        DrawBevelFrame(canvas, sr, theme.bevel, /*sunken=*/p.pressed && !p.disabled, /*thin=*/false);

        const Color fg = p.disabled ? pal.textDisabled : pal.textPrimary;
        const keel::f32 shift = GetButtonContentOffset(theme, p.pressed, false, p.disabled);
        const keel::f32 gw = keel::min(9.0f, keel::max(6.0f, sr.w - 8.0f));
        const keel::f32 gh = keel::min(8.0f, keel::max(6.0f, sr.h - 8.0f));
        const Rect gr(sr.Center().x - gw * 0.5f + shift, sr.Center().y - gh * 0.5f + shift, gw, gh);
        switch (p.kind) {
            case WindowControlKind::Minimize:
                DrawWinCtrlMinimizeGlyph(canvas, gr, fg, true);
                break;
            case WindowControlKind::Maximize:
                if (p.maximized) {
                    DrawWinCtrlRestoreGlyph(canvas, gr, fg, true);
                } else {
                    DrawWinCtrlMaximizeGlyph(canvas, gr, fg, true);
                }
                break;
            case WindowControlKind::Close:
                DrawWinCtrlCloseGlyph(canvas, gr, fg, true);
                break;
        }
        return;
    }

    // Modern / Fluent / Luminous / Glass / Brutalist caption buttons
    Color bg = Color::Transparent();
    Color fg = p.disabled
        ? pal.textDisabled
        : (p.focused ? pal.textSecondary : pal.textMuted);

    if (!p.disabled) {
        if (p.kind == WindowControlKind::Close) {
            if (p.hovered || p.pressed) {
                const Color dangerBg = theme.GetSemanticAccent(UiVariant::Danger);
                bg = p.pressed ? dangerBg.WithAlphaF(0.85f) : dangerBg;
                fg = theme.EnsureReadableInk(pal.accentText, dangerBg);
            }
        } else {
            if (p.pressed) {
                bg = pal.surfaceActive;
                fg = pal.textPrimary;
            } else if (p.hovered) {
                bg = pal.surfaceHover;
                fg = pal.textPrimary;
            }
        }
    }

    if (bg.a > 0u || (!p.disabled && (p.hovered || p.pressed))) {
        DrawFrame(canvas, theme, FrameKind::ToolbarButton, FramePaintParams{
            .rect = r,
            .radius = keel::min(4.0f, theme.GetRadius(UiSize::Sm)),
            .bg = bg,
            .hovered = p.hovered,
            .pressed = p.pressed,
            .disabled = p.disabled,
            .accent = (p.kind == WindowControlKind::Close) ? theme.GetSemanticAccent(UiVariant::Danger) : pal.accent
        });
    }

    const keel::f32 glyphSz = 10.0f;
    const Rect gr(r.Center().x - glyphSz * 0.5f, r.Center().y - glyphSz * 0.5f, glyphSz, glyphSz);
    switch (p.kind) {
        case WindowControlKind::Minimize:
            DrawWinCtrlMinimizeGlyph(canvas, gr, fg, false);
            break;
        case WindowControlKind::Maximize:
            if (p.maximized) {
                DrawWinCtrlRestoreGlyph(canvas, gr, fg, false);
            } else {
                DrawWinCtrlMaximizeGlyph(canvas, gr, fg, false);
            }
            break;
        case WindowControlKind::Close:
            DrawWinCtrlCloseGlyph(canvas, gr, fg, false);
            break;
    }
}

const IStylePainter& GetDefaultStylePainter() noexcept {
    static const DefaultStylePainter kDefaultPainter{};
    return kDefaultPainter;
}

} // namespace guikit
