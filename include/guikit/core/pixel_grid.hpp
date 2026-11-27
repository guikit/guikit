#pragma once

#include "guikit/backend/i_canvas.hpp"
#include "keel/math.h"

namespace guikit {

/// Physical framebuffer pixel-grid quantization helper for crisp, DPI-consistent frames and lines.
///
/// Maps logical coordinates through `ICanvas::GetPixelTransform()` into physical pixels, snaps
/// edges and stroke thicknesses to exact whole physical pixels, and maps back to logical units.
/// Because snapped edges land on integer physical pixel boundaries (`N.0 px`), every edge sits
/// a full `0.5 px` away from pixel centers (`N ± 0.5 px`), eliminating subpixel thickness jitter
/// across fractional DPI scales (`125%`, `150%`, `175%`), UI zoom, and subpixel container offsets.
struct PixelGrid {
    /// Integer physical-pixel rectangle `[x0..x1) x [y0..y1)` for zero-drift multi-layer frame geometry.
    struct SnappedPxRect {
        keel::i32 x0{0};
        keel::i32 y0{0};
        keel::i32 x1{0};
        keel::i32 y1{0};

        [[nodiscard]] constexpr keel::i32 Width() const noexcept { return (x1 > x0) ? (x1 - x0) : 0; }
        [[nodiscard]] constexpr keel::i32 Height() const noexcept { return (y1 > y0) ? (y1 - y0) : 0; }
        [[nodiscard]] constexpr bool Empty() const noexcept { return x1 <= x0 || y1 <= y0; }
    };

    Transform2D toPx{1.0f, Vec2{0.0f, 0.0f}};
    keel::f32 invScale{1.0f};

    [[nodiscard]] static PixelGrid From(const ICanvas& canvas) noexcept {
        return FromTransform(canvas.GetPixelTransform());
    }

    [[nodiscard]] static PixelGrid FromTransform(const Transform2D& pixelTransform) noexcept {
        // Below 75% scale (extreme zoom-out / thumbnails), 1 physical pixel would exceed 1.33 dp
        // and bloat multi-layer bevels; fall back to logical 1:1 grid so geometry stays proportional.
        if (pixelTransform.scale < 0.75f) {
            return PixelGrid{Transform2D{1.0f, Vec2{0.0f, 0.0f}}, 1.0f};
        }
        return PixelGrid{
            Transform2D{pixelTransform.scale, pixelTransform.offset},
            1.0f / pixelTransform.scale
        };
    }

    /// Physical pixel count for a nominal 1 dp hairline / bevel step.
    /// Uses `floor(scale + 0.25)` clamped >= 1:
    ///   100% -> 1 px, 125% -> 1 px, 150% -> 1 px, 175% -> 2 px, 200% -> 2 px.
    /// Keeps 2-layer 3D bevels crisp and strictly inside `frameBevelInset` at 150%
    /// while stepping up cleanly to 2 px on 175%..200% (4K / Retina).
    [[nodiscard]] keel::i32 UnitStepPx() const noexcept {
        return keel::max(keel::i32{1}, keel::as<keel::i32>(keel::floor(toPx.scale + 0.25f)));
    }

    /// Physical pixel count for an arbitrary stroke width in logical dp.
    /// Nominal 1 dp strokes (`<= 1.05 dp`) follow `UnitStepPx()`; thicker strokes round normally.
    [[nodiscard]] keel::i32 StrokePx(keel::f32 dpWidth) const noexcept {
        if (dpWidth <= 0.0f) return 0;
        if (dpWidth <= 1.05f) return UnitStepPx();
        return keel::max(keel::i32{1}, keel::as<keel::i32>(keel::floor(dpWidth * toPx.scale + 0.5f)));
    }

    /// Converts an integer count of physical pixels into logical dp units.
    [[nodiscard]] keel::f32 PxToDp(keel::i32 px) const noexcept {
        return keel::as<keel::f32>(px) * invScale;
    }

    /// Logical dp thickness of a single nominal 1 dp bevel/border step.
    [[nodiscard]] keel::f32 UnitStepDp() const noexcept {
        return PxToDp(UnitStepPx());
    }

    /// Logical dp thickness of a quantized stroke.
    [[nodiscard]] keel::f32 StrokeDp(keel::f32 dpWidth) const noexcept {
        return PxToDp(StrokePx(dpWidth));
    }

    /// Snaps a logical X coordinate to the nearest integer physical pixel boundary.
    [[nodiscard]] keel::i32 SnapXToPx(keel::f32 x) const noexcept {
        return keel::as<keel::i32>(keel::floor(x * toPx.scale + toPx.offset.x + 0.5f));
    }

    /// Snaps a logical Y coordinate to the nearest integer physical pixel boundary.
    [[nodiscard]] keel::i32 SnapYToPx(keel::f32 y) const noexcept {
        return keel::as<keel::i32>(keel::floor(y * toPx.scale + toPx.offset.y + 0.5f));
    }

    /// Maps an integer physical X boundary back into logical coordinates.
    [[nodiscard]] keel::f32 PxToLogicalX(keel::i32 px) const noexcept {
        return (keel::as<keel::f32>(px) - toPx.offset.x) * invScale;
    }

    /// Maps an integer physical Y boundary back into logical coordinates.
    [[nodiscard]] keel::f32 PxToLogicalY(keel::i32 py) const noexcept {
        return (keel::as<keel::f32>(py) - toPx.offset.y) * invScale;
    }

    /// Snaps a logical X coordinate to an integer physical pixel boundary (in logical units).
    [[nodiscard]] keel::f32 SnapEdgeX(keel::f32 x) const noexcept {
        return PxToLogicalX(SnapXToPx(x));
    }

    /// Snaps a logical Y coordinate to an integer physical pixel boundary (in logical units).
    [[nodiscard]] keel::f32 SnapEdgeY(keel::f32 y) const noexcept {
        return PxToLogicalY(SnapYToPx(y));
    }

    /// Snaps a logical X center for a vertical line of thickness `strokePx` physical pixels:
    /// odd thicknesses (`1, 3, ...`) align to pixel centers (`N + 0.5`), even (`2, 4, ...`) to boundaries (`N`).
    [[nodiscard]] keel::f32 SnapLineCenterX(keel::f32 x, keel::i32 strokePx) const noexcept {
        const keel::f32 px = x * toPx.scale + toPx.offset.x;
        const keel::f32 snappedPx = ((strokePx & 1) != 0)
            ? (keel::floor(px) + 0.5f)
            : keel::floor(px + 0.5f);
        return (snappedPx - toPx.offset.x) * invScale;
    }

    /// Snaps a logical Y center for a horizontal line of thickness `strokePx` physical pixels.
    [[nodiscard]] keel::f32 SnapLineCenterY(keel::f32 y, keel::i32 strokePx) const noexcept {
        const keel::f32 py = y * toPx.scale + toPx.offset.y;
        const keel::f32 snappedPy = ((strokePx & 1) != 0)
            ? (keel::floor(py) + 0.5f)
            : keel::floor(py + 0.5f);
        return (snappedPy - toPx.offset.y) * invScale;
    }

    /// Snaps all four edges of `r` to integer physical pixel boundaries.
    [[nodiscard]] SnappedPxRect SnapRectPx(Rect r) const noexcept {
        const keel::i32 x0 = SnapXToPx(r.x);
        const keel::i32 y0 = SnapYToPx(r.y);
        const keel::i32 x1 = (r.w > 0.0f) ? keel::max(x0 + 1, SnapXToPx(r.Right())) : x0;
        const keel::i32 y1 = (r.h > 0.0f) ? keel::max(y0 + 1, SnapYToPx(r.Bottom())) : y0;
        return SnappedPxRect{x0, y0, x1, y1};
    }

    /// Converts an integer physical-pixel rectangle `[px0..px1) x [py0..py1)` back into a logical `Rect`.
    [[nodiscard]] Rect PxRectToLogical(keel::i32 px0, keel::i32 py0, keel::i32 px1, keel::i32 py1) const noexcept {
        const keel::i32 w = (px1 > px0) ? (px1 - px0) : 0;
        const keel::i32 h = (py1 > py0) ? (py1 - py0) : 0;
        return Rect(
            PxToLogicalX(px0),
            PxToLogicalY(py0),
            PxToDp(w),
            PxToDp(h)
        );
    }

    /// Converts a `SnappedPxRect` back into a logical `Rect`.
    [[nodiscard]] Rect PxRectToLogical(const SnappedPxRect& sr) const noexcept {
        return PxRectToLogical(sr.x0, sr.y0, sr.x1, sr.y1);
    }

    /// Snaps all four edges of `r` to physical pixel boundaries and returns the logical `Rect`.
    [[nodiscard]] Rect SnapRect(Rect r) const noexcept {
        return PxRectToLogical(SnapRectPx(r));
    }
};

} // namespace guikit
