#include "guikit/widgets/AttitudeView.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Control.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kDegToRad = kPi / 180.0f;

struct AttitudeViewState {
    float fovPitchDeg{50.0f};
    bool initialized{false};
};

struct AttitudeViewPayload {
    AttitudeViewOptions opts{};
    AttitudeViewResponse resp{};
};

struct AttitudePalette {
    Color bgConsole{10, 14, 24, 255};
    Color bezelRing{30, 41, 59, 255};
    Color bezelHighlight{100, 116, 139, 140};
    Color skyFill{4, 118, 208, 255};
    Color skyGlow{68, 178, 250, 85};
    Color groundFill{122, 66, 24, 255};
    Color groundGlow{168, 98, 42, 75};
    Color horizonLine{255, 255, 255, 255};
    Color pitchLadder{248, 250, 252, 242};
    Color bankScale{248, 250, 252, 235};
    Color chevronAlert{244, 63, 94, 245};
    Color aircraftFill{250, 204, 21, 255};
    Color aircraftStroke{10, 14, 24, 245};
    Color flightDirector{236, 72, 246, 255};
    Color fpvColor{52, 245, 118, 250};
    Color tapeBg{13, 19, 33, 215};
    Color tapeBorder{148, 163, 184, 150};
    Color tapeTick{241, 245, 249, 230};
    Color bugCyan{34, 211, 238, 255};
    Color ilsMagenta{236, 72, 246, 255};
    Color fmaGreen{74, 222, 128, 255};
    Color fmaCyan{56, 189, 248, 255};
    Color fmaWhite{248, 250, 252, 250};
    Color amberWarn{251, 191, 36, 255};
    Color redLimit{239, 68, 68, 240};
    Color greenArc{34, 197, 94, 210};
};

[[nodiscard]] AttitudePalette BuildPalette(AttitudeColorScheme scheme, AttitudeDisplayMode mode) noexcept {
    AttitudePalette p{};
    if (scheme == AttitudeColorScheme::HudGreen || mode == AttitudeDisplayMode::HudOverlay) {
        const bool isHudMode = (mode == AttitudeDisplayMode::HudOverlay);
        p.bgConsole = isHudMode ? Color(5, 14, 10, 235) : Color(6, 16, 11, 255);
        p.bezelRing = Color(16, 48, 28, 220);
        p.bezelHighlight = Color(52, 245, 118, 110);
        p.skyFill = isHudMode ? Color(0, 0, 0, 0) : Color(8, 30, 18, 255);
        p.skyGlow = isHudMode ? Color(52, 245, 118, 18) : Color(52, 245, 118, 30);
        p.groundFill = isHudMode ? Color(0, 0, 0, 0) : Color(14, 42, 22, 255);
        p.groundGlow = isHudMode ? Color(52, 245, 118, 12) : Color(52, 245, 118, 22);
        const Color hudMain{52, 245, 118, 245};
        const Color hudBright{167, 255, 198, 255};
        p.horizonLine = hudBright;
        p.pitchLadder = hudMain;
        p.bankScale = hudMain;
        p.chevronAlert = Color(250, 204, 21, 245);
        p.aircraftFill = hudBright;
        p.aircraftStroke = Color(4, 16, 8, 220);
        p.flightDirector = hudBright;
        p.fpvColor = hudBright;
        p.tapeBg = Color(4, 18, 10, 175);
        p.tapeBorder = Color(52, 245, 118, 140);
        p.tapeTick = hudMain;
        p.bugCyan = hudBright;
        p.ilsMagenta = hudBright;
        p.fmaGreen = hudBright;
        p.fmaCyan = hudMain;
        p.fmaWhite = hudMain;
        p.amberWarn = Color(250, 204, 21, 245);
        p.redLimit = Color(244, 63, 94, 235);
        p.greenArc = hudMain;
    } else if (scheme == AttitudeColorScheme::TacticalDark) {
        p.bgConsole = Color(8, 11, 20, 255);
        p.bezelRing = Color(22, 30, 48, 255);
        p.bezelHighlight = Color(56, 189, 248, 120);
        p.skyFill = Color(15, 38, 82, 255);
        p.skyGlow = Color(56, 189, 248, 65);
        p.groundFill = Color(52, 36, 26, 255);
        p.groundGlow = Color(180, 110, 52, 55);
        p.horizonLine = Color(56, 189, 248, 250);
    }
    return p;
}

[[nodiscard]] inline float NormalizeHeadingDeg(float deg) noexcept {
    float r = std::fmod(deg, 360.0f);
    if (r < 0.0f) r += 360.0f;
    return r;
}

[[nodiscard]] inline float SignedAngleDiffDeg(float a, float b) noexcept {
    return std::fmod(a - b + 540.0f, 360.0f) - 180.0f;
}

/// Clips a convex polygon against the half-plane `(P - planeOrigin) . planeNormal <= 0`.
[[nodiscard]] std::vector<Vec2> ClipPolygonHalfPlane(
    const std::vector<Vec2>& poly,
    Vec2 planeOrigin,
    Vec2 planeNormal
) {
    std::vector<Vec2> out;
    if (poly.empty()) return out;
    out.reserve(poly.size() + 2);

    auto evalDist = [&](Vec2 pt) noexcept -> float {
        return (pt.x - planeOrigin.x) * planeNormal.x + (pt.y - planeOrigin.y) * planeNormal.y;
    };

    Vec2 prev = poly.back();
    float prevDist = evalDist(prev);

    for (const Vec2 curr : poly) {
        const float currDist = evalDist(curr);
        const bool prevInside = (prevDist <= 0.0f);
        const bool currInside = (currDist <= 0.0f);

        if (prevInside != currInside) {
            const float denom = prevDist - currDist;
            const float t = (std::abs(denom) > 1e-6f) ? (prevDist / denom) : 0.5f;
            out.push_back(Vec2{
                prev.x + t * (curr.x - prev.x),
                prev.y + t * (curr.y - prev.y)
            });
        }
        if (currInside) {
            out.push_back(curr);
        }
        prev = curr;
        prevDist = currDist;
    }
    return out;
}

void DrawConvexPolygonFilled(ICanvas& canvas, const std::vector<Vec2>& poly, Color fill) noexcept {
    if (poly.size() < 3 || fill.a == 0) return;
    Vec2 center{0.0f, 0.0f};
    for (const Vec2& p : poly) {
        center.x += p.x;
        center.y += p.y;
    }
    const float invN = 1.0f / static_cast<float>(poly.size());
    center.x *= invN;
    center.y *= invN;

    for (size_t i = 0; i < poly.size(); ++i) {
        const Vec2& a = poly[i];
        const Vec2& b = poly[(i + 1) % poly.size()];
        canvas.DrawTriangle(center, a, b, fill);
    }
}

/// Builds a rounded rectangle or circular convex polygon representing the ADI sphere boundary.
[[nodiscard]] std::vector<Vec2> BuildAdiBoundaryPolygon(Rect adiRect, bool circular) {
    std::vector<Vec2> pts;
    const Vec2 c = adiRect.Center();
    if (circular) {
        constexpr int kSteps = 48;
        pts.reserve(kSteps);
        const float r = std::min(adiRect.w, adiRect.h) * 0.5f;
        for (int i = 0; i < kSteps; ++i) {
            const float a = (static_cast<float>(i) / kSteps) * 2.0f * kPi;
            pts.push_back({c.x + std::cos(a) * r, c.y + std::sin(a) * r});
        }
        return pts;
    }

    const float cornerR = std::min(adiRect.w, adiRect.h) * 0.22f;
    const float x0 = adiRect.x + cornerR;
    const float x1 = adiRect.Right() - cornerR;
    const float y0 = adiRect.y + cornerR;
    const float y1 = adiRect.Bottom() - cornerR;
    constexpr int kArcSteps = 8;
    pts.reserve(kArcSteps * 4 + 4);

    const std::array<Vec2, 4> centers = {
        Vec2{x1, y0}, // Top-Right
        Vec2{x1, y1}, // Bottom-Right
        Vec2{x0, y1}, // Bottom-Left
        Vec2{x0, y0}  // Top-Left
    };
    const std::array<float, 4> startAngles = {-0.5f * kPi, 0.0f, 0.5f * kPi, kPi};

    for (int corner = 0; corner < 4; ++corner) {
        for (int s = 0; s <= kArcSteps; ++s) {
            const float a = startAngles[corner] + (static_cast<float>(s) / kArcSteps) * (0.5f * kPi);
            pts.push_back({
                centers[corner].x + std::cos(a) * cornerR,
                centers[corner].y + std::sin(a) * cornerR
            });
        }
    }
    return pts;
}

/// Clips a 2D line segment (p0, p1) to a circle (center, radius). Returns true if any part is inside.
[[nodiscard]] bool ClipSegmentToCircle(Vec2& p0, Vec2& p1, Vec2 center, float radius) noexcept {
    const Vec2 d{p1.x - p0.x, p1.y - p0.y};
    const Vec2 f{p0.x - center.x, p0.y - center.y};
    const float a = d.x * d.x + d.y * d.y;
    if (a < 1e-6f) {
        return (f.x * f.x + f.y * f.y) <= radius * radius;
    }
    const float b = 2.0f * (f.x * d.x + f.y * d.y);
    const float c = (f.x * f.x + f.y * f.y) - radius * radius;
    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return false;
    disc = std::sqrt(disc);
    const float t0 = (-b - disc) / (2.0f * a);
    const float t1 = (-b + disc) / (2.0f * a);
    if (t1 < 0.0f || t0 > 1.0f) return false;
    const float cl0 = std::clamp(t0, 0.0f, 1.0f);
    const float cl1 = std::clamp(t1, 0.0f, 1.0f);
    if (cl1 <= cl0) return false;
    const Vec2 orig0 = p0;
    p0 = {orig0.x + cl0 * d.x, orig0.y + cl0 * d.y};
    p1 = {orig0.x + cl1 * d.x, orig0.y + cl1 * d.y};
    return true;
}

void DrawCircleOutline(ICanvas& canvas, Vec2 center, float radius, Color stroke, float strokeWidth = 1.5f, int steps = 36) noexcept {
    if (radius <= 0.5f || stroke.a == 0) return;
    Vec2 prev{center.x + radius, center.y};
    for (int i = 1; i <= steps; ++i) {
        const float a = (static_cast<float>(i) / steps) * 2.0f * kPi;
        const Vec2 curr{center.x + std::cos(a) * radius, center.y + std::sin(a) * radius};
        canvas.DrawLine(prev, curr, stroke, strokeWidth);
        prev = curr;
    }
}

void DrawFilledCircle(ICanvas& canvas, Vec2 center, float radius, Color fill, Color stroke = Color::Transparent(), float strokeWidth = 0.0f, int steps = 28) noexcept {
    if (radius <= 0.5f) return;
    if (fill.a > 0) {
        Vec2 prev{center.x + radius, center.y};
        for (int i = 1; i <= steps; ++i) {
            const float a = (static_cast<float>(i) / steps) * 2.0f * kPi;
            const Vec2 curr{center.x + std::cos(a) * radius, center.y + std::sin(a) * radius};
            canvas.DrawTriangle(center, prev, curr, fill);
            prev = curr;
        }
    }
    if (stroke.a > 0 && strokeWidth > 0.0f) {
        DrawCircleOutline(canvas, center, radius, stroke, strokeWidth, steps);
    }
}

void DrawQuadFilled(ICanvas& canvas, Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, Color fill, Color stroke = Color::Transparent(), float strokeW = 1.0f) noexcept {
    if (fill.a > 0) {
        canvas.DrawTriangle(p0, p1, p2, fill);
        canvas.DrawTriangle(p0, p2, p3, fill);
    }
    if (stroke.a > 0 && strokeW > 0.0f) {
        canvas.DrawLine(p0, p1, stroke, strokeW);
        canvas.DrawLine(p1, p2, stroke, strokeW);
        canvas.DrawLine(p2, p3, stroke, strokeW);
        canvas.DrawLine(p3, p0, stroke, strokeW);
    }
}

/// Draws the Pitch Ladder (2.5 / 5 / 10 deg bars, negative dashed bars, zenith/nadir stars, recovery chevrons).
void DrawPitchLadder(
    ICanvas& canvas,
    Vec2 center,
    float clipRadius,
    float pitchDeg,
    float horizonRollDeg,
    float pxPerDeg,
    const AttitudePalette& pal,
    bool isHud
) noexcept {
    const float phi = horizonRollDeg * kDegToRad;
    // Unit vector along horizon (left to right when unrolled)
    const Vec2 u{std::cos(phi), -std::sin(phi)};
    // Unit normal pointing into ground (downward when unrolled)
    const Vec2 n{std::sin(phi), std::cos(phi)};

    const float ladderClipR = clipRadius * 0.78f;

    auto toScreen = [&](float lateralPx, float ladderPitchDeg) noexcept -> Vec2 {
        const float distAlongNormal = (pitchDeg - ladderPitchDeg) * pxPerDeg;
        return Vec2{
            center.x + lateralPx * u.x + distAlongNormal * n.x,
            center.y + lateralPx * u.y + distAlongNormal * n.y
        };
    };

    auto drawClippedLine = [&](Vec2 a, Vec2 b, Color col, float w) noexcept {
        if (ClipSegmentToCircle(a, b, center, ladderClipR)) {
            canvas.DrawLine(a, b, col, w);
        }
    };

    for (int halfSteps = -34; halfSteps <= 34; ++halfSteps) {
        if (halfSteps == 0) continue; // Horizon line is drawn across full sphere
        const float deg = static_cast<float>(halfSteps) * 2.5f;
        if (std::abs(deg - pitchDeg) * pxPerDeg > ladderClipR * 1.15f) continue;

        const bool isTen = (halfSteps % 4 == 0);
        const bool isFive = (halfSteps % 2 == 0) && !isTen;
        const bool isTwoHalf = !isTen && !isFive;

        if (isTwoHalf && std::abs(deg) > 20.0f) continue;

        const float innerGap = isTen ? (clipRadius * 0.12f) : (isFive ? (clipRadius * 0.09f) : (clipRadius * 0.05f));
        const float outerSpan = isTen ? (clipRadius * 0.36f) : (isFive ? (clipRadius * 0.22f) : (clipRadius * 0.12f));
        const float tipLen = isTen ? (clipRadius * 0.055f) : (isFive ? (clipRadius * 0.04f) : 0.0f);
        const float lineW = isTen ? 1.8f : 1.3f;

        // Tips point toward the horizon: for deg > 0, toward +n (downward); for deg < 0, toward -n (upward)
        const float tipPitchDelta = (deg > 0.0f) ? (-tipLen / pxPerDeg) : (+tipLen / pxPerDeg);

        for (int side = -1; side <= 1; side += 2) {
            const float s = static_cast<float>(side);
            const float xInner = s * innerGap;
            const float xOuter = s * outerSpan;

            if (deg > 0.0f || isTwoHalf) {
                // Solid pitch bar above horizon (or minor 2.5 deg tick)
                drawClippedLine(toScreen(xInner, deg), toScreen(xOuter, deg), pal.pitchLadder, lineW);
            } else {
                // Dashed pitch bar below horizon (2 segments per side)
                const float mid1 = xInner + (xOuter - xInner) * 0.42f;
                const float mid2 = xInner + (xOuter - xInner) * 0.58f;
                drawClippedLine(toScreen(xInner, deg), toScreen(mid1, deg), pal.pitchLadder, lineW);
                drawClippedLine(toScreen(mid2, deg), toScreen(xOuter, deg), pal.pitchLadder, lineW);
            }

            if (tipLen > 0.5f) {
                // Vertical horizon-pointing tip at outer end (or inner end in HUD mode)
                const float tipX = isHud ? xInner : xOuter;
                drawClippedLine(toScreen(tipX, deg), toScreen(tipX, deg + tipPitchDelta), pal.pitchLadder, lineW);
            }
        }

        // Degree labels on both sides of 10-degree bars
        if (isTen) {
            const int absDeg = static_cast<int>(std::round(std::abs(deg)));
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d", absDeg);
            const float fontSize = std::clamp(clipRadius * 0.072f, 9.5f, 12.5f);
            const Vec2 textSz = canvas.MeasureText(buf, fontSize);

            for (int side = -1; side <= 1; side += 2) {
                const float labelX = static_cast<float>(side) * (outerSpan + textSz.x * 0.7f + 5.0f);
                const Vec2 pt = toScreen(labelX, deg);
                const float dx = pt.x - center.x;
                const float dy = pt.y - center.y;
                if (dx * dx + dy * dy <= ladderClipR * ladderClipR) {
                    canvas.DrawText(buf, {pt.x - textSz.x * 0.5f, pt.y - textSz.y * 0.5f}, fontSize, pal.pitchLadder);
                }
            }
        }

        // Unusual Attitude Recovery Chevrons at +-35, +-45, +-65 deg pointing toward horizon
        if (halfSteps == 14 || halfSteps == -14 || halfSteps == 18 || halfSteps == -18 || halfSteps == 26 || halfSteps == -26) {
            const float chevW = clipRadius * 0.14f;
            const float chevH = (deg > 0.0f) ? -3.2f : +3.2f; // Points toward horizon (0 deg)
            const Vec2 apex = toScreen(0.0f, deg + chevH);
            const Vec2 left = toScreen(-chevW, deg);
            const Vec2 right = toScreen(+chevW, deg);
            drawClippedLine(left, apex, pal.chevronAlert, 2.4f);
            drawClippedLine(right, apex, pal.chevronAlert, 2.4f);
        }
    }

    // Zenith (+90 deg) and Nadir (-90 deg) reticles
    for (int pole = -1; pole <= 1; pole += 2) {
        const float poleDeg = static_cast<float>(pole) * 90.0f;
        const Vec2 pt = toScreen(0.0f, poleDeg);
        const float dx = pt.x - center.x;
        const float dy = pt.y - center.y;
        if (dx * dx + dy * dy < ladderClipR * ladderClipR) {
            const float r = clipRadius * 0.055f;
            DrawCircleOutline(canvas, pt, r, pal.pitchLadder, 1.5f, 20);
            if (pole > 0) {
                canvas.DrawLine({pt.x - r * 1.4f, pt.y}, {pt.x + r * 1.4f, pt.y}, pal.pitchLadder, 1.3f);
                canvas.DrawLine({pt.x, pt.y - r * 1.4f}, {pt.x, pt.y + r * 1.4f}, pal.pitchLadder, 1.3f);
            }
        }
    }
}

/// Draws the upper Bank Scale Arc (10, 20, 30, 45, 60 deg), rotating Roll Pointer, and Slip/Skid Inclinometer.
void DrawBankScale(
    ICanvas& canvas,
    Vec2 center,
    float radius,
    float rollDeg,
    float slipSkid,
    HorizonReferenceMode refMode,
    const AttitudePalette& pal
) noexcept {
    const float arcR = radius * 0.85f;

    // 1. Upper bank arc from -60 to +60 deg (where 0 deg is straight up at -pi/2)
    constexpr int kArcSegments = 36;
    Vec2 prev{
        center.x + std::sin(-60.0f * kDegToRad) * arcR,
        center.y - std::cos(-60.0f * kDegToRad) * arcR
    };
    for (int i = 1; i <= kArcSegments; ++i) {
        const float deg = -60.0f + (static_cast<float>(i) / kArcSegments) * 120.0f;
        const float rad = deg * kDegToRad;
        const Vec2 curr{
            center.x + std::sin(rad) * arcR,
            center.y - std::cos(rad) * arcR
        };
        canvas.DrawLine(prev, curr, pal.bankScale, 1.5f);
        prev = curr;
    }

    // 2. Bank scale radial ticks at +-10, +-20, +-30, +-45, +-60 deg
    constexpr std::array<int, 10> kTicks = {-60, -45, -30, -20, -10, 10, 20, 30, 45, 60};
    for (int tDeg : kTicks) {
        const float rad = static_cast<float>(tDeg) * kDegToRad;
        const float s = std::sin(rad);
        const float c = std::cos(rad);
        const bool isMajor = (std::abs(tDeg) == 30 || std::abs(tDeg) == 60);
        const bool isFortyFive = (std::abs(tDeg) == 45);
        const float tickLen = isMajor ? (radius * 0.068f) : (radius * 0.038f);

        const Vec2 p0{center.x + s * arcR, center.y - c * arcR};
        const Vec2 p1{center.x + s * (arcR + tickLen), center.y - c * (arcR + tickLen)};

        if (isFortyFive) {
            // Small hollow triangle/diamond at 45 deg
            const Vec2 tip = p0;
            const Vec2 ortho{c, s};
            const float halfW = radius * 0.018f;
            const Vec2 bL{p1.x - ortho.x * halfW, p1.y - ortho.y * halfW};
            const Vec2 bR{p1.x + ortho.x * halfW, p1.y + ortho.y * halfW};
            canvas.DrawTriangle(tip, bL, bR, pal.bankScale);
        } else {
            canvas.DrawLine(p0, p1, pal.bankScale, isMajor ? 2.0f : 1.4f);
        }
    }

    // 3. Fixed top 0-degree sky reference triangle (pointing down to arcR)
    const float triH = radius * 0.052f;
    const float triW = radius * 0.032f;
    const Vec2 fixedApex{center.x, center.y - arcR};
    const Vec2 fixedTL{center.x - triW, center.y - arcR - triH};
    const Vec2 fixedTR{center.x + triW, center.y - arcR - triH};
    canvas.DrawTriangle(fixedApex, fixedTL, fixedTR, pal.aircraftFill, pal.aircraftStroke, 1.0f);

    // 4. Moving Roll Pointer Triangle & Slip/Skid Trapezoid ("Ball") inside the arc
    const float pointerAngleDeg = (refMode == HorizonReferenceMode::WesternMovingHorizon) ? -rollDeg : rollDeg;
    const float pRad = pointerAngleDeg * kDegToRad;
    const Vec2 radialUp{std::sin(pRad), -std::cos(pRad)};
    const Vec2 tangent{std::cos(pRad), std::sin(pRad)};

    const float pTipR = arcR - 2.0f;
    const float pBaseR = pTipR - triH;
    const Vec2 pApex{center.x + radialUp.x * pTipR, center.y + radialUp.y * pTipR};
    const Vec2 pBaseL{
        center.x + radialUp.x * pBaseR - tangent.x * triW,
        center.y + radialUp.y * pBaseR - tangent.y * triW
    };
    const Vec2 pBaseR_pt{
        center.x + radialUp.x * pBaseR + tangent.x * triW,
        center.y + radialUp.y * pBaseR + tangent.y * triW
    };
    const Color ptrColor = ( std::abs(rollDeg) > 45.0f ) ? pal.amberWarn : pal.bankScale;
    canvas.DrawTriangle(pApex, pBaseL, pBaseR_pt, ptrColor, pal.aircraftStroke, 1.0f);

    // Slip/Skid Trapezoid directly beneath the roll pointer base
    const float slipPx = std::clamp(slipSkid, -1.0f, 1.0f) * (triW * 2.4f);
    const float trapTopR = pBaseR - 2.5f;
    const float trapBotR = trapTopR - radius * 0.024f;
    const float topW = triW * 0.88f;
    const float botW = triW * 1.08f;

    const Vec2 t0{
        center.x + radialUp.x * trapTopR + tangent.x * (slipPx - topW),
        center.y + radialUp.y * trapTopR + tangent.y * (slipPx - topW)
    };
    const Vec2 t1{
        center.x + radialUp.x * trapTopR + tangent.x * (slipPx + topW),
        center.y + radialUp.y * trapTopR + tangent.y * (slipPx + topW)
    };
    const Vec2 t2{
        center.x + radialUp.x * trapBotR + tangent.x * (slipPx + botW),
        center.y + radialUp.y * trapBotR + tangent.y * (slipPx + botW)
    };
    const Vec2 t3{
        center.x + radialUp.x * trapBotR + tangent.x * (slipPx - botW),
        center.y + radialUp.y * trapBotR + tangent.y * (slipPx - botW)
    };
    DrawQuadFilled(canvas, t0, t1, t2, t3, ptrColor, pal.aircraftStroke, 1.0f);
}

/// Draws Flight Path Vector ("bird") and Flight Director (Single-Cue V-Bar or Dual-Cue Crossbars).
void DrawFlightDirectorAndFpv(
    ICanvas& canvas,
    Vec2 center,
    float radius,
    const AttitudeViewOptions& opts,
    float pxPerDeg,
    const AttitudePalette& pal
) noexcept {
    // 1. Flight Path Vector (FPV / Velocity Vector)
    if (opts.showFlightPathVector) {
        const float fpvPitch = opts.pitchDeg - opts.aoaDeg;
        const float dy = (opts.pitchDeg - fpvPitch) * pxPerDeg; // +aoaDeg shifts bird down when nose is pitched above velocity vector
        const float dx = opts.sideslipDeg * pxPerDeg;
        const Vec2 fpvPos{
            std::clamp(center.x + dx, center.x - radius * 0.7f, center.x + radius * 0.7f),
            std::clamp(center.y + dy, center.y - radius * 0.7f, center.y + radius * 0.7f)
        };
        const float r = std::clamp(radius * 0.042f, 5.5f, 9.0f);
        DrawCircleOutline(canvas, fpvPos, r, pal.fpvColor, 2.0f, 24);
        // Left & right horizontal wings
        canvas.DrawLine({fpvPos.x - r * 2.5f, fpvPos.y}, {fpvPos.x - r, fpvPos.y}, pal.fpvColor, 2.0f);
        canvas.DrawLine({fpvPos.x + r, fpvPos.y}, {fpvPos.x + r * 2.5f, fpvPos.y}, pal.fpvColor, 2.0f);
        // Top vertical stabilizer fin
        canvas.DrawLine({fpvPos.x, fpvPos.y - r}, {fpvPos.x, fpvPos.y - r * 2.1f}, pal.fpvColor, 2.0f);
    }

    // 2. Flight Director (FD)
    if (opts.flightDirectorStyle == FlightDirectorStyle::Off) return;

    const float fdDy = (opts.pitchDeg - opts.fdPitchDeg) * pxPerDeg;
    const float fdRollDiff = (opts.fdRollDeg - opts.rollDeg);

    if (opts.flightDirectorStyle == FlightDirectorStyle::SingleCueVBar) {
        // Magenta V-Bar command wedges
        const Vec2 apex{
            center.x,
            std::clamp(center.y + fdDy, center.y - radius * 0.62f, center.y + radius * 0.62f)
        };
        const float rotRad = fdRollDiff * kDegToRad;
        const Vec2 u{std::cos(rotRad), std::sin(rotRad)};
        const Vec2 n{-std::sin(rotRad), std::cos(rotRad)};

        const float wingSpan = radius * 0.34f;
        const float wingDrop = radius * 0.085f;
        const float wingThick = radius * 0.036f;

        for (int side = -1; side <= 1; side += 2) {
            const float s = static_cast<float>(side);
            const Vec2 pTip{
                apex.x + s * (radius * 0.035f) * u.x,
                apex.y + s * (radius * 0.035f) * u.y
            };
            const Vec2 pOuterTop{
                apex.x + s * wingSpan * u.x + (wingDrop - wingThick) * n.x,
                apex.y + s * wingSpan * u.y + (wingDrop - wingThick) * n.y
            };
            const Vec2 pOuterBot{
                apex.x + s * wingSpan * u.x + (wingDrop + wingThick) * n.x,
                apex.y + s * wingSpan * u.y + (wingDrop + wingThick) * n.y
            };
            canvas.DrawTriangle(pTip, pOuterTop, pOuterBot, pal.flightDirector, pal.aircraftStroke, 1.2f);
        }
    } else if (opts.flightDirectorStyle == FlightDirectorStyle::DualCueCrossbars) {
        // Dual-cue horizontal pitch bar & vertical roll bar
        const float barSpan = radius * 0.42f;
        const float pitchY = std::clamp(center.y + fdDy, center.y - barSpan, center.y + barSpan);
        const float rollX = std::clamp(center.x + fdRollDiff * pxPerDeg * 0.7f, center.x - barSpan, center.x + barSpan);

        canvas.DrawLine({center.x - barSpan, pitchY}, {center.x + barSpan, pitchY}, pal.aircraftStroke, 4.4f);
        canvas.DrawLine({center.x - barSpan, pitchY}, {center.x + barSpan, pitchY}, pal.flightDirector, 2.6f);

        canvas.DrawLine({rollX, center.y - barSpan}, {rollX, center.y + barSpan}, pal.aircraftStroke, 4.4f);
        canvas.DrawLine({rollX, center.y - barSpan}, {rollX, center.y + barSpan}, pal.flightDirector, 2.6f);
    }
}

/// Draws the central Aircraft Reference Symbol (GullWing, ClassicW, or RussianSilhouette).
void DrawAircraftReferenceSymbol(
    ICanvas& canvas,
    Vec2 center,
    float radius,
    const AttitudeViewOptions& opts,
    const AttitudePalette& pal
) noexcept {
    // In WesternMovingHorizon, aircraft symbol stays level (0 deg).
    // In EasternMovingAircraft (KPPI), aircraft symbol rotates by +rollDeg.
    const float symRollDeg = (opts.referenceMode == HorizonReferenceMode::EasternMovingAircraft)
        ? opts.rollDeg
        : 0.0f;
    const float rad = symRollDeg * kDegToRad;
    const Vec2 u{std::cos(rad), std::sin(rad)};
    const Vec2 n{-std::sin(rad), std::cos(rad)};

    auto pt = [&](float x, float y) noexcept -> Vec2 {
        return Vec2{
            center.x + x * u.x + y * n.x,
            center.y + x * u.y + y * n.y
        };
    };

    if (opts.aircraftSymbol == AircraftSymbolStyle::GullWing) {
        // Center bore-sight square
        const float sq = std::max(3.5f, radius * 0.025f);
        DrawQuadFilled(canvas, pt(-sq, -sq), pt(sq, -sq), pt(sq, sq), pt(-sq, sq), pal.aircraftFill, pal.aircraftStroke, 1.4f);

        // Left & Right L-shaped gull wings
        const float innerX = radius * 0.14f;
        const float outerX = radius * 0.38f;
        const float barH = std::max(3.2f, radius * 0.022f);
        const float dropH = radius * 0.065f;

        for (int side = -1; side <= 1; side += 2) {
            const float s = static_cast<float>(side);
            // Horizontal wing bar
            DrawQuadFilled(
                canvas,
                pt(s * innerX, -barH),
                pt(s * outerX, -barH),
                pt(s * outerX, +barH),
                pt(s * innerX, +barH),
                pal.aircraftFill,
                pal.aircraftStroke,
                1.3f
            );
            // Inner vertical drop tab
            DrawQuadFilled(
                canvas,
                pt(s * innerX, +barH),
                pt(s * (innerX + barH * 2.0f), +barH),
                pt(s * (innerX + barH * 2.0f), +dropH),
                pt(s * innerX, +dropH),
                pal.aircraftFill,
                pal.aircraftStroke,
                1.3f
            );
        }
    } else if (opts.aircraftSymbol == AircraftSymbolStyle::ClassicW) {
        const std::array<Vec2, 7> wPts = {
            pt(-radius * 0.36f, 0.0f),
            pt(-radius * 0.16f, 0.0f),
            pt(-radius * 0.08f, radius * 0.07f),
            pt(0.0f, 0.0f),
            pt(+radius * 0.08f, radius * 0.07f),
            pt(+radius * 0.16f, 0.0f),
            pt(+radius * 0.36f, 0.0f)
        };
        for (size_t i = 0; i + 1 < wPts.size(); ++i) {
            canvas.DrawLine(wPts[i], wPts[i + 1], pal.aircraftStroke, 5.0f);
        }
        for (size_t i = 0; i + 1 < wPts.size(); ++i) {
            canvas.DrawLine(wPts[i], wPts[i + 1], pal.aircraftFill, 2.8f);
        }
    } else {
        // Russian KPPI silhouette: wide wings + fuselage ring + vertical tail fin
        const float rCenter = std::max(5.0f, radius * 0.042f);
        const float wingOuter = radius * 0.42f;
        canvas.DrawLine(pt(-wingOuter, 0.0f), pt(-rCenter, 0.0f), pal.aircraftStroke, 5.2f);
        canvas.DrawLine(pt(+rCenter, 0.0f), pt(+wingOuter, 0.0f), pal.aircraftStroke, 5.2f);
        canvas.DrawLine(pt(0.0f, -rCenter), pt(0.0f, -radius * 0.14f), pal.aircraftStroke, 5.2f);

        canvas.DrawLine(pt(-wingOuter, 0.0f), pt(-rCenter, 0.0f), pal.aircraftFill, 3.0f);
        canvas.DrawLine(pt(+rCenter, 0.0f), pt(+wingOuter, 0.0f), pal.aircraftFill, 3.0f);
        canvas.DrawLine(pt(0.0f, -rCenter), pt(0.0f, -radius * 0.14f), pal.aircraftFill, 3.0f);
        DrawCircleOutline(canvas, center, rCenter, pal.aircraftFill, 2.6f, 24);
        DrawFilledCircle(canvas, center, 2.2f, pal.aircraftFill);
    }
}

/// Draws the left Indicated Airspeed (IAS) scrolling tape, bug, speed limits, and Mach box.
void DrawAirspeedTape(
    ICanvas& canvas,
    Rect tapeRect,
    const AttitudeViewOptions& opts,
    const AttitudePalette& pal
) noexcept {
    if (tapeRect.w < 24.0f || tapeRect.h < 60.0f) return;

    const float headerH = 22.0f;
    const float footerH = 22.0f;
    const Rect bodyRect(tapeRect.x, tapeRect.y + headerH, tapeRect.w, std::max(30.0f, tapeRect.h - headerH - footerH));
    const float midY = bodyRect.y + bodyRect.h * 0.5f;
    const float pxPerKnot = bodyRect.h / 90.0f; // 90 knots visible span (+-45 kt)

    // Top selected airspeed bug readout box
    const Rect topBox(tapeRect.x, tapeRect.y, tapeRect.w, headerH - 2.0f);
    canvas.DrawRoundRect(topBox, 3.0f, Color(9, 13, 24, 235), pal.tapeBorder, 1.0f);
    char bugBuf[24];
    std::snprintf(bugBuf, sizeof(bugBuf), "%03.0f", std::max(0.0f, opts.targetAirspeedBug));
    canvas.DrawTextBox(bugBuf, topBox, 11.5f, pal.bugCyan, 0.5f, 0.5f);

    // Main scrolling tape body
    canvas.DrawRoundRect(bodyRect, 2.0f, pal.tapeBg, pal.tapeBorder, 1.0f);
    canvas.PushClip(bodyRect);

    const float curIas = std::max(0.0f, opts.airspeedKnots);
    const int minKt = std::max(0, static_cast<int>(std::floor((curIas - 55.0f) / 5.0f)) * 5);
    const int maxKt = static_cast<int>(std::ceil((curIas + 55.0f) / 5.0f)) * 5;

    // Speed envelope colored strips along right edge of tape (Red stall < 110 kt, Green 135..310 kt, Red Vmo > 330 kt)
    auto ktToY = [&](float kt) noexcept -> float {
        return midY - (kt - curIas) * pxPerKnot;
    };

    const float stripX = bodyRect.Right() - 4.5f;
    const float yStallTop = std::clamp(ktToY(115.0f), bodyRect.y, bodyRect.Bottom());
    const float yStallBot = std::clamp(ktToY(0.0f), bodyRect.y, bodyRect.Bottom());
    if (yStallBot > yStallTop) {
        canvas.DrawRect(Rect(stripX, yStallTop, 3.5f, yStallBot - yStallTop), pal.redLimit);
    }

    const float yGreenTop = std::clamp(ktToY(315.0f), bodyRect.y, bodyRect.Bottom());
    const float yGreenBot = std::clamp(ktToY(135.0f), bodyRect.y, bodyRect.Bottom());
    if (yGreenBot > yGreenTop) {
        canvas.DrawRect(Rect(stripX, yGreenTop, 2.5f, yGreenBot - yGreenTop), pal.greenArc);
    }

    const float yVmoTop = std::clamp(ktToY(999.0f), bodyRect.y, bodyRect.Bottom());
    const float yVmoBot = std::clamp(ktToY(330.0f), bodyRect.y, bodyRect.Bottom());
    if (yVmoBot > yVmoTop) {
        canvas.DrawRect(Rect(stripX, yVmoTop, 3.5f, yVmoBot - yVmoTop), pal.redLimit);
    }

    // Ticks every 5 knots, numbers every 10 knots
    for (int kt = minKt; kt <= maxKt; kt += 5) {
        const float y = ktToY(static_cast<float>(kt));
        if (y < bodyRect.y - 8.0f || y > bodyRect.Bottom() + 8.0f) continue;

        const bool isTen = (kt % 10 == 0);
        const float tickW = isTen ? 9.0f : 5.0f;
        canvas.DrawLine({bodyRect.Right() - tickW, y}, {bodyRect.Right(), y}, pal.tapeTick, isTen ? 1.4f : 1.0f);

        if (isTen) {
            char kBuf[16];
            std::snprintf(kBuf, sizeof(kBuf), "%d", kt);
            canvas.DrawTextBox(
                kBuf,
                Rect(bodyRect.x + 3.0f, y - 8.0f, bodyRect.w - tickW - 6.0f, 16.0f),
                10.5f,
                pal.tapeTick,
                1.0f,
                0.5f
            );
        }
    }

    // Autopilot Selected Speed Bug on right edge
    const float bugY = std::clamp(ktToY(opts.targetAirspeedBug), bodyRect.y + 5.0f, bodyRect.Bottom() - 5.0f);
    canvas.DrawTriangle(
        {bodyRect.Right() - 8.0f, bugY},
        {bodyRect.Right(), bugY - 5.5f},
        {bodyRect.Right(), bugY + 5.5f},
        pal.bugCyan
    );

    // Center Rolling-Drum Digital Value Box
    const float drumH = 24.0f;
    const Rect drumRect(bodyRect.x + 1.0f, midY - drumH * 0.5f, bodyRect.w - 6.0f, drumH);
    canvas.DrawRoundRect(drumRect, 2.0f, Color(6, 9, 16, 250), pal.fmaWhite, 1.5f);
    canvas.DrawTriangle(
        {bodyRect.Right() - 1.0f, midY},
        {drumRect.Right(), midY - 5.0f},
        {drumRect.Right(), midY + 5.0f},
        pal.fmaWhite
    );
    char curBuf[16];
    std::snprintf(curBuf, sizeof(curBuf), "%03.0f", curIas);
    canvas.DrawTextBox(curBuf, drumRect, 13.0f, pal.fmaWhite, 0.5f, 0.5f);

    canvas.PopClip();

    // Bottom Mach Number Box
    const Rect botBox(tapeRect.x, bodyRect.Bottom() + 2.0f, tapeRect.w, footerH - 2.0f);
    canvas.DrawRoundRect(botBox, 3.0f, Color(9, 13, 24, 235), pal.tapeBorder, 1.0f);
    char machBuf[24];
    std::snprintf(machBuf, sizeof(machBuf), "M .%03d", std::clamp(static_cast<int>(std::round(opts.machNumber * 1000.0f)), 0, 999));
    canvas.DrawTextBox(machBuf, botBox, 10.5f, pal.fmaGreen, 0.5f, 0.5f);
}

/// Draws the right Barometric Altitude scrolling tape, Baro box, and Vertical Speed Indicator (VSI) column.
void DrawAltitudeAndVsiTape(
    ICanvas& canvas,
    Rect altGroupRect,
    const AttitudeViewOptions& opts,
    const AttitudePalette& pal
) noexcept {
    if (altGroupRect.w < 32.0f || altGroupRect.h < 60.0f) return;

    const float vsiW = std::clamp(altGroupRect.w * 0.30f, 22.0f, 32.0f);
    const float altW = std::max(28.0f, altGroupRect.w - vsiW - 3.0f);
    const float headerH = 22.0f;
    const float footerH = 22.0f;

    const Rect altBody(altGroupRect.x, altGroupRect.y + headerH, altW, std::max(30.0f, altGroupRect.h - headerH - footerH));
    const float midY = altBody.y + altBody.h * 0.5f;
    const float pxPerFoot = altBody.h / 900.0f; // +-450 ft visible span

    // 1. Top Selected Altitude Bug Readout Box
    const Rect topBox(altGroupRect.x, altGroupRect.y, altGroupRect.w, headerH - 2.0f);
    canvas.DrawRoundRect(topBox, 3.0f, Color(9, 13, 24, 235), pal.tapeBorder, 1.0f);
    char altBugBuf[24];
    std::snprintf(altBugBuf, sizeof(altBugBuf), "%05.0f", opts.targetAltitudeBug);
    canvas.DrawTextBox(altBugBuf, topBox, 11.5f, pal.bugCyan, 0.5f, 0.5f);

    // 2. Main Scrolling Altitude Tape
    canvas.DrawRoundRect(altBody, 2.0f, pal.tapeBg, pal.tapeBorder, 1.0f);
    canvas.PushClip(altBody);

    const float curAlt = opts.altitudeFeet;
    const int minFt = static_cast<int>(std::floor((curAlt - 550.0f) / 20.0f)) * 20;
    const int maxFt = static_cast<int>(std::ceil((curAlt + 550.0f) / 20.0f)) * 20;

    auto ftToY = [&](float ft) noexcept -> float {
        return midY - (ft - curAlt) * pxPerFoot;
    };

    for (int ft = minFt; ft <= maxFt; ft += 20) {
        const float y = ftToY(static_cast<float>(ft));
        if (y < altBody.y - 8.0f || y > altBody.Bottom() + 8.0f) continue;

        const bool isHundred = (ft % 100 == 0);
        const float tickW = isHundred ? 8.5f : 4.5f;
        canvas.DrawLine({altBody.x, y}, {altBody.x + tickW, y}, pal.tapeTick, isHundred ? 1.4f : 1.0f);

        if (isHundred) {
            char fBuf[16];
            std::snprintf(fBuf, sizeof(fBuf), "%d", ft);
            canvas.DrawTextBox(
                fBuf,
                Rect(altBody.x + tickW + 3.0f, y - 8.0f, altBody.w - tickW - 5.0f, 16.0f),
                10.0f,
                pal.tapeTick,
                0.0f,
                0.5f
            );
        }
    }

    // Selected Altitude Bug on left edge of altitude tape
    const float bugY = std::clamp(ftToY(opts.targetAltitudeBug), altBody.y + 5.0f, altBody.Bottom() - 5.0f);
    canvas.DrawTriangle(
        {altBody.x + 8.0f, bugY},
        {altBody.x, bugY - 5.5f},
        {altBody.x, bugY + 5.5f},
        pal.bugCyan
    );

    // Center Rolling-Drum Digital Altitude Readout Box
    const float drumH = 24.0f;
    const Rect drumRect(altBody.x + 5.0f, midY - drumH * 0.5f, altBody.w - 6.0f, drumH);
    canvas.DrawRoundRect(drumRect, 2.0f, Color(6, 9, 16, 250), pal.fmaWhite, 1.5f);
    canvas.DrawTriangle(
        {altBody.x + 1.0f, midY},
        {drumRect.x, midY - 5.0f},
        {drumRect.x, midY + 5.0f},
        pal.fmaWhite
    );
    char curAltBuf[16];
    std::snprintf(curAltBuf, sizeof(curAltBuf), "%05.0f", curAlt);
    canvas.DrawTextBox(curAltBuf, drumRect, 12.0f, pal.fmaWhite, 0.5f, 0.5f);

    canvas.PopClip();

    // 3. Vertical Speed Indicator (VSI) Column on the right
    const Rect vsiRect(altBody.Right() + 3.0f, altBody.y + 12.0f, vsiW, std::max(24.0f, altBody.h - 24.0f));
    canvas.DrawRoundRect(vsiRect, 2.0f, pal.tapeBg, pal.tapeBorder, 1.0f);
    canvas.PushClip(vsiRect);

    const float vsiMidY = vsiRect.y + vsiRect.h * 0.5f;
    const float vsiHalfH = (vsiRect.h * 0.5f) - 8.0f;

    // Piecewise non-linear VSI mapping (-4000 .. +4000 fpm)
    auto fpmToY = [&](float fpm) noexcept -> float {
        const float clamped = std::clamp(fpm, -4000.0f, 4000.0f);
        const float sign = (clamped >= 0.0f) ? 1.0f : -1.0f;
        const float a = std::abs(clamped);
        float norm = 0.0f;
        if (a <= 1000.0f) {
            norm = (a / 1000.0f) * 0.45f;
        } else if (a <= 2000.0f) {
            norm = 0.45f + ((a - 1000.0f) / 1000.0f) * 0.30f;
        } else {
            norm = 0.75f + ((a - 2000.0f) / 2000.0f) * 0.25f;
        }
        return vsiMidY - sign * norm * vsiHalfH;
    };

    constexpr std::array<int, 7> kVsiMarks = {-4000, -2000, -1000, 0, 1000, 2000, 4000};
    for (int fpm : kVsiMarks) {
        const float y = fpmToY(static_cast<float>(fpm));
        const bool isZero = (fpm == 0);
        canvas.DrawLine({vsiRect.x, y}, {vsiRect.x + (isZero ? 8.0f : 5.0f), y}, pal.tapeTick, isZero ? 1.6f : 1.1f);
        if (fpm != 0) {
            char vBuf[8];
            std::snprintf(vBuf, sizeof(vBuf), "%d", std::abs(fpm) / 1000);
            canvas.DrawText(vBuf, {vsiRect.x + 6.5f, y - 5.5f}, 9.0f, pal.tapeTick);
        }
    }

    // VSI Needle from right edge to current vertical speed Y
    const float needleY = fpmToY(opts.verticalSpeedFpm);
    canvas.DrawLine({vsiRect.Right() - 2.0f, vsiMidY}, {vsiRect.x + 2.0f, needleY}, pal.fmaGreen, 2.2f);
    canvas.PopClip();

    // 4. Bottom Barometric Setting Box (29.92 IN or STD)
    const Rect botBox(altGroupRect.x, altBody.Bottom() + 2.0f, altGroupRect.w, footerH - 2.0f);
    canvas.DrawRoundRect(botBox, 3.0f, Color(9, 13, 24, 235), pal.tapeBorder, 1.0f);
    char baroBuf[24];
    if (opts.baroStd) {
        std::snprintf(baroBuf, sizeof(baroBuf), "STD BARO");
    } else {
        std::snprintf(baroBuf, sizeof(baroBuf), "%.2f IN", opts.baroSettingInHg);
    }
    canvas.DrawTextBox(baroBuf, botBox, 10.5f, pal.bugCyan, 0.5f, 0.5f);
}

/// Draws the bottom magnetic compass Heading Tape and Selected Heading Bug.
void DrawHeadingTape(
    ICanvas& canvas,
    Rect hdgRect,
    const AttitudeViewOptions& opts,
    const AttitudePalette& pal
) noexcept {
    if (hdgRect.w < 60.0f || hdgRect.h < 18.0f) return;

    canvas.DrawRoundRect(hdgRect, 2.0f, pal.tapeBg, pal.tapeBorder, 1.0f);
    canvas.PushClip(hdgRect);

    const float curHdg = NormalizeHeadingDeg(opts.headingDeg);
    const float midX = hdgRect.x + hdgRect.w * 0.5f;
    const float pxPerDeg = hdgRect.w / 70.0f; // +-35 deg visible compass span

    const int baseFive = static_cast<int>(std::floor(curHdg / 5.0f)) * 5;
    for (int offset = -40; offset <= 40; offset += 5) {
        const int rawDeg = baseFive + offset;
        const float diff = SignedAngleDiffDeg(static_cast<float>(rawDeg), curHdg);
        const float x = midX + diff * pxPerDeg;
        if (x < hdgRect.x - 10.0f || x > hdgRect.Right() + 10.0f) continue;

        const int normDeg = ((rawDeg % 360) + 360) % 360;
        const bool isTen = (normDeg % 10 == 0);
        const bool isCardinal = (normDeg % 90 == 0);
        const float tickH = isTen ? 7.5f : 4.0f;

        canvas.DrawLine({x, hdgRect.y}, {x, hdgRect.y + tickH}, pal.tapeTick, isTen ? 1.4f : 1.0f);

        if (isTen) {
            char hBuf[12];
            if (normDeg == 0) std::snprintf(hBuf, sizeof(hBuf), "N");
            else if (normDeg == 90) std::snprintf(hBuf, sizeof(hBuf), "E");
            else if (normDeg == 180) std::snprintf(hBuf, sizeof(hBuf), "S");
            else if (normDeg == 270) std::snprintf(hBuf, sizeof(hBuf), "W");
            else std::snprintf(hBuf, sizeof(hBuf), "%02d", normDeg / 10);

            canvas.DrawTextBox(
                hBuf,
                Rect(x - 14.0f, hdgRect.y + tickH + 1.0f, 28.0f, hdgRect.h - tickH - 2.0f),
                isCardinal ? 11.0f : 10.0f,
                isCardinal ? pal.fmaCyan : pal.tapeTick,
                0.5f,
                0.5f
            );
        }
    }

    // Selected Heading Bug on top edge
    const float bugDiff = SignedAngleDiffDeg(opts.targetHeadingBug, curHdg);
    const float bugX = std::clamp(midX + bugDiff * pxPerDeg, hdgRect.x + 6.0f, hdgRect.Right() - 6.0f);
    canvas.DrawTriangle(
        {bugX, hdgRect.y + 8.0f},
        {bugX - 5.5f, hdgRect.y},
        {bugX + 5.5f, hdgRect.y},
        pal.bugCyan
    );

    // Fixed Center Lubber Triangle & Digital Box
    canvas.DrawTriangle(
        {midX, hdgRect.y + 6.5f},
        {midX - 5.0f, hdgRect.y},
        {midX + 5.0f, hdgRect.y},
        pal.aircraftFill
    );

    canvas.PopClip();
}

/// Draws ILS Localizer (bottom) and Glideslope (right) 4-dot deviation scales and magenta diamonds.
void DrawIlsDeviationScales(
    ICanvas& canvas,
    Rect adiRect,
    const AttitudeViewOptions& opts,
    const AttitudePalette& pal
) noexcept {
    const Vec2 c = adiRect.Center();

    // 1. Horizontal Localizer Scale near bottom of ADI
    const float locY = adiRect.Bottom() - 12.0f;
    const float locHalfW = std::min(88.0f, adiRect.w * 0.34f);
    const Rect locBar(c.x - locHalfW - 8.0f, locY - 7.0f, (locHalfW + 8.0f) * 2.0f, 14.0f);
    canvas.DrawRoundRect(locBar, 4.0f, Color(9, 13, 24, 195), pal.tapeBorder, 1.0f);

    canvas.DrawLine({c.x, locY - 5.0f}, {c.x, locY + 5.0f}, pal.fmaWhite, 1.6f);
    for (int dot = -2; dot <= 2; ++dot) {
        if (dot == 0) continue;
        const float dx = (static_cast<float>(dot) / 2.0f) * locHalfW;
        DrawCircleOutline(canvas, {c.x + dx, locY}, 2.4f, pal.tapeTick, 1.2f, 12);
    }
    const float locX = c.x + (std::clamp(opts.locDeviationDots, -2.2f, 2.2f) / 2.0f) * locHalfW;
    DrawQuadFilled(
        canvas,
        {locX, locY - 5.0f},
        {locX + 5.5f, locY},
        {locX, locY + 5.0f},
        {locX - 5.5f, locY},
        pal.ilsMagenta
    );

    // 2. Vertical Glideslope Scale along right edge of ADI
    const float gsX = adiRect.Right() - 12.0f;
    const float gsHalfH = std::min(88.0f, adiRect.h * 0.34f);
    const Rect gsBar(gsX - 7.0f, c.y - gsHalfH - 8.0f, 14.0f, (gsHalfH + 8.0f) * 2.0f);
    canvas.DrawRoundRect(gsBar, 4.0f, Color(9, 13, 24, 195), pal.tapeBorder, 1.0f);

    canvas.DrawLine({gsX - 5.0f, c.y}, {gsX + 5.0f, c.y}, pal.fmaWhite, 1.6f);
    for (int dot = -2; dot <= 2; ++dot) {
        if (dot == 0) continue;
        const float dy = (static_cast<float>(dot) / 2.0f) * gsHalfH;
        DrawCircleOutline(canvas, {gsX, c.y + dy}, 2.4f, pal.tapeTick, 1.2f, 12);
    }
    const float gsY = c.y - (std::clamp(opts.gsDeviationDots, -2.2f, 2.2f) / 2.0f) * gsHalfH;
    DrawQuadFilled(
        canvas,
        {gsX, gsY - 5.5f},
        {gsX + 5.0f, gsY},
        {gsX, gsY + 5.5f},
        {gsX - 5.0f, gsY},
        pal.ilsMagenta
    );
}

/// Draws the top 5-column Flight Mode Annunciator (FMA) status bar.
void DrawFmaBar(
    ICanvas& canvas,
    Rect fmaRect,
    const AttitudeViewOptions& opts,
    const AttitudePalette& pal
) noexcept {
    if (fmaRect.w < 120.0f || fmaRect.h < 18.0f) return;
    canvas.DrawRoundRect(fmaRect, 3.0f, pal.tapeBg, pal.tapeBorder, 1.0f);

    const float colW = fmaRect.w / 5.0f;
    for (int i = 1; i < 5; ++i) {
        const float x = fmaRect.x + colW * static_cast<float>(i);
        canvas.DrawLine({x, fmaRect.y + 3.0f}, {x, fmaRect.Bottom() - 3.0f}, pal.tapeBorder, 1.0f);
    }

    const char* pitchMode = (std::abs(opts.altitudeFeet - opts.targetAltitudeBug) < 80.0f) ? "ALT HOLD" : "VNAV PTH";
    const char* rollMode = (opts.showIlsDiamonds && std::abs(opts.locDeviationDots) < 1.5f) ? "LOC*" : "HDG SEL";
    const char* fdLabel = (opts.flightDirectorStyle != FlightDirectorStyle::Off) ? "AP1  1FD2" : "MAN  A/THR";

    canvas.DrawTextBox("MACH / SPD", Rect(fmaRect.x, fmaRect.y, colW, fmaRect.h), 10.5f, pal.fmaGreen, 0.5f, 0.5f);
    canvas.DrawTextBox(pitchMode, Rect(fmaRect.x + colW, fmaRect.y, colW, fmaRect.h), 10.5f, pal.fmaGreen, 0.5f, 0.5f);
    canvas.DrawTextBox(rollMode, Rect(fmaRect.x + colW * 2.0f, fmaRect.y, colW, fmaRect.h), 10.5f, pal.fmaGreen, 0.5f, 0.5f);
    canvas.DrawTextBox("CAT III DUAL", Rect(fmaRect.x + colW * 3.0f, fmaRect.y, colW, fmaRect.h), 10.0f, pal.fmaCyan, 0.5f, 0.5f);
    canvas.DrawTextBox(fdLabel, Rect(fmaRect.x + colW * 4.0f, fmaRect.y, colW, fmaRect.h), 10.5f, pal.fmaWhite, 0.5f, 0.5f);
}

void RenderAttitudeViewNode(const Node* node, ICanvas& canvas, const UiTheme& theme) noexcept {
    const auto* payload = node->As<AttitudeViewPayload>();
    if (!payload) return;

    const AttitudeViewOptions& opts = payload->opts;
    const AttitudeViewResponse& resp = payload->resp;

    const Rect r = node->computedRect;
    if (r.w <= 16.0f || r.h <= 16.0f) return;

    const AttitudePalette pal = BuildPalette(opts.colorScheme, opts.displayMode);
    const float radius = opts.border.radius.has_value()
        ? theme.Scale(*opts.border.radius)
        : theme.GetRadius(node->uiSize);
    const Color bgConsole = opts.background.Resolve(node->hovered, node->pressed, node->disabled, pal.bgConsole);
    const Color borderCol = opts.border.color.Resolve(node->hovered, node->pressed, node->disabled, theme.palette.borderSubtle);
    const float borderW = (opts.border.width > 0.0f) ? theme.Scale(opts.border.width) : 1.0f;

    canvas.DrawRoundRect(r, radius, bgConsole, borderCol, borderW);
    canvas.PushClip(r);

    // Compute sub-instrument rectangles inside `r`
    const bool isFullPfd = (opts.displayMode == AttitudeDisplayMode::FullPFD);
    const bool isCompact = (opts.displayMode == AttitudeDisplayMode::CompactADI);
    const bool isHud = (opts.displayMode == AttitudeDisplayMode::HudOverlay);

    const float pad = 8.0f;
    const float fmaH = (opts.showFmaBar && !isCompact && r.h > 220.0f) ? 26.0f : 0.0f;
    const float hdgH = (opts.showHeadingTape && !isCompact && r.h > 220.0f) ? 30.0f : 0.0f;
    const float leftTapeW = (isFullPfd && opts.showSpeedAndAltTapes && r.w > 300.0f)
        ? std::clamp(r.w * 0.14f, 56.0f, 76.0f)
        : 0.0f;
    const float rightTapeW = (isFullPfd && opts.showSpeedAndAltTapes && r.w > 300.0f)
        ? std::clamp(r.w * 0.19f, 76.0f, 106.0f)
        : 0.0f;

    const float contentTop = r.y + pad + (fmaH > 0.0f ? (fmaH + 6.0f) : 0.0f);
    const float contentBot = r.Bottom() - pad - (hdgH > 0.0f ? (hdgH + 6.0f) : 0.0f);
    const float contentH = std::max(40.0f, contentBot - contentTop);

    const float adiLeft = r.x + pad + (leftTapeW > 0.0f ? (leftTapeW + 8.0f) : 0.0f);
    const float adiRight = r.Right() - pad - (rightTapeW > 0.0f ? (rightTapeW + 8.0f) : 0.0f);
    const float availAdiW = std::max(40.0f, adiRight - adiLeft);

    const float adiSide = std::min(availAdiW, contentH);
    const Rect adiRect(
        adiLeft + (availAdiW - adiSide) * 0.5f,
        contentTop + (contentH - adiSide) * 0.5f,
        adiSide,
        adiSide
    );
    const Vec2 adiCenter = adiRect.Center();
    const float adiRadius = adiSide * 0.5f;

    // 1. Artificial Horizon Sphere (Sky & Ground Half-Plane Clipped Polygons)
    const float horizonRollDeg = (opts.referenceMode == HorizonReferenceMode::WesternMovingHorizon)
        ? opts.rollDeg
        : 0.0f;
    const float fovPitch = std::clamp(resp.visibleFovPitchDeg, 25.0f, 90.0f);
    const float pxPerDeg = (2.0f * adiRadius) / fovPitch;

    const float phi = horizonRollDeg * kDegToRad;
    const Vec2 u{std::cos(phi), -std::sin(phi)};
    const Vec2 nGround{std::sin(phi), std::cos(phi)};
    const Vec2 nSky{-nGround.x, -nGround.y};

    const Vec2 horizonOrigin{
        adiCenter.x + (opts.pitchDeg * pxPerDeg) * nGround.x,
        adiCenter.y + (opts.pitchDeg * pxPerDeg) * nGround.y
    };

    const std::vector<Vec2> boundaryPoly = BuildAdiBoundaryPolygon(adiRect, isCompact);

    if (!isHud) {
        // Clip boundary polygon into Sky (nGround <= 0) and Ground (nSky <= 0)
        const std::vector<Vec2> skyPoly = ClipPolygonHalfPlane(boundaryPoly, horizonOrigin, nGround);
        const std::vector<Vec2> groundPoly = ClipPolygonHalfPlane(boundaryPoly, horizonOrigin, nSky);

        DrawConvexPolygonFilled(canvas, skyPoly, pal.skyFill);
        DrawConvexPolygonFilled(canvas, groundPoly, pal.groundFill);

        // Subtle atmospheric horizon glow bands (+/- 20 px around horizon)
        const float glowBandPx = std::min(22.0f, adiRadius * 0.16f);
        const Vec2 skyGlowLimit{
            horizonOrigin.x - glowBandPx * nGround.x,
            horizonOrigin.y - glowBandPx * nGround.y
        };
        const Vec2 groundGlowLimit{
            horizonOrigin.x + glowBandPx * nGround.x,
            horizonOrigin.y + glowBandPx * nGround.y
        };
        const std::vector<Vec2> skyGlowPoly = ClipPolygonHalfPlane(skyPoly, skyGlowLimit, nSky);
        const std::vector<Vec2> groundGlowPoly = ClipPolygonHalfPlane(groundPoly, groundGlowLimit, nGround);
        DrawConvexPolygonFilled(canvas, skyGlowPoly, pal.skyGlow);
        DrawConvexPolygonFilled(canvas, groundGlowPoly, pal.groundGlow);
    }

    // Horizon Dividing Line clipped to the ADI boundary polygon
    {
        const float span = adiRadius * 1.45f;
        Vec2 h0{horizonOrigin.x - span * u.x, horizonOrigin.y - span * u.y};
        Vec2 h1{horizonOrigin.x + span * u.x, horizonOrigin.y + span * u.y};
        if (isCompact) {
            if (ClipSegmentToCircle(h0, h1, adiCenter, adiRadius)) {
                canvas.DrawLine(h0, h1, pal.horizonLine, 2.2f);
            }
        } else {
            canvas.PushClip(adiRect);
            canvas.DrawLine(h0, h1, pal.horizonLine, 2.2f);
            canvas.PopClip();
        }
    }

    // 2. Pitch Ladder, Zenith/Nadir Reticles & Recovery Chevrons
    if (opts.showPitchLadder) {
        canvas.PushClip(adiRect);
        DrawPitchLadder(canvas, adiCenter, adiRadius, opts.pitchDeg, horizonRollDeg, pxPerDeg, pal, isHud);
        canvas.PopClip();
    }

    // 3. Upper Bank Scale Arc, Roll Pointer & Slip/Skid Ball
    if (opts.showBankScale) {
        DrawBankScale(canvas, adiCenter, adiRadius, opts.rollDeg, opts.slipSkid, opts.referenceMode, pal);
    }

    // 4. Flight Path Vector (FPV) & Flight Director (FD)
    canvas.PushClip(adiRect);
    DrawFlightDirectorAndFpv(canvas, adiCenter, adiRadius, opts, pxPerDeg, pal);

    // 5. Radio Altimeter AGL Readout near bottom of ADI sphere (when < 2500 ft)
    if (opts.radioAltitudeFeet < 2500.0f) {
        const float raY = adiRect.Bottom() - (opts.showIlsDiamonds && isFullPfd ? 34.0f : 20.0f);
        const Rect raBox(adiCenter.x - 36.0f, raY - 9.0f, 72.0f, 18.0f);
        canvas.DrawRoundRect(raBox, 3.0f, Color(6, 9, 16, 215), pal.tapeBorder, 1.0f);
        char raBuf[24];
        std::snprintf(raBuf, sizeof(raBuf), "RA %4.0f", std::max(0.0f, opts.radioAltitudeFeet));
        const Color raCol = (opts.radioAltitudeFeet < 400.0f) ? pal.amberWarn : pal.fmaGreen;
        canvas.DrawTextBox(raBuf, raBox, 10.5f, raCol, 0.5f, 0.5f);
    }

    // 6. Central Aircraft Reference Symbol
    DrawAircraftReferenceSymbol(canvas, adiCenter, adiRadius, opts, pal);

    // 7. Interactive Mouse / Virtual Stick Vector Overlay
    if (opts.showMouseStickOverlay && resp.cursorInsideAdi) {
        const Vec2 stickPt{
            adiCenter.x + resp.cursorNorm.x * (adiRadius * 0.78f),
            adiCenter.y + resp.cursorNorm.y * (adiRadius * 0.78f)
        };
        canvas.DrawLine(adiCenter, stickPt, Color(56, 189, 248, 145), 1.4f);
        DrawCircleOutline(canvas, stickPt, 7.0f, Color(56, 189, 248, 235), 1.6f, 20);
        DrawFilledCircle(canvas, stickPt, 2.2f, Color(250, 204, 21, 245));
    }
    canvas.PopClip();

    // 8. Outer ADI Bezel Frame (Circular ring in CompactADI, Rounded dome frame in FullPFD/HUD)
    if (isCompact) {
        DrawCircleOutline(canvas, adiCenter, adiRadius, pal.bezelRing, 4.0f, 64);
        DrawCircleOutline(canvas, adiCenter, adiRadius + 1.5f, pal.bezelHighlight, 1.2f, 64);
    } else {
        const float domeRadius = adiSide * 0.22f;
        canvas.DrawRoundRect(adiRect, domeRadius, Color::Transparent(), pal.bezelHighlight, 1.5f);
    }

    // 9. ILS Localizer & Glideslope Deviation Diamonds
    if (opts.showIlsDiamonds && !isCompact) {
        DrawIlsDeviationScales(canvas, adiRect, opts, pal);
    }

    // 10. Top FMA Strip, Left Airspeed Tape, Right Altitude/VSI Tape, Bottom Heading Tape
    if (fmaH > 0.0f) {
        const Rect fmaRect(r.x + pad, r.y + pad, r.w - pad * 2.0f, fmaH);
        DrawFmaBar(canvas, fmaRect, opts, pal);
    }

    if (leftTapeW > 0.0f) {
        const Rect iasRect(r.x + pad, adiRect.y, leftTapeW, adiRect.h);
        DrawAirspeedTape(canvas, iasRect, opts, pal);
    }

    if (rightTapeW > 0.0f) {
        const Rect altRect(r.Right() - pad - rightTapeW, adiRect.y, rightTapeW, adiRect.h);
        DrawAltitudeAndVsiTape(canvas, altRect, opts, pal);
    }

    if (hdgH > 0.0f) {
        const Rect hdgRect(adiRect.x, r.Bottom() - pad - hdgH, adiRect.w, hdgH);
        DrawHeadingTape(canvas, hdgRect, opts, pal);
    }

    // 11. Corner Digital Telemetry Readouts (in CompactADI or HUD mode when side tapes are hidden)
    if (leftTapeW <= 0.0f) {
        char tlBuf[32];
        std::snprintf(tlBuf, sizeof(tlBuf), "IAS %3.0f KT", opts.airspeedKnots);
        canvas.DrawText(tlBuf, {r.x + 12.0f, r.y + 10.0f}, 11.0f, pal.fmaGreen);

        char trBuf[32];
        std::snprintf(trBuf, sizeof(trBuf), "ALT %5.0f FT", opts.altitudeFeet);
        const float trW = canvas.MeasureText(trBuf, 11.0f).x;
        canvas.DrawText(trBuf, {r.Right() - trW - 12.0f, r.y + 10.0f}, 11.0f, pal.fmaCyan);
    }

    // Bottom corner attitude & G-load summary tags
    {
        char blBuf[48];
        std::snprintf(blBuf, sizeof(blBuf), "BNK %+05.1f°  PIT %+05.1f°", opts.rollDeg, opts.pitchDeg);
        canvas.DrawText(blBuf, {r.x + 12.0f, r.Bottom() - 18.0f}, 10.0f, pal.tapeTick);

        char brBuf[48];
        std::snprintf(brBuf, sizeof(brBuf), "HDG %03.0f°  G %+.1f", NormalizeHeadingDeg(opts.headingDeg), opts.gLoad);
        const float brW = canvas.MeasureText(brBuf, 10.0f).x;
        canvas.DrawText(brBuf, {r.Right() - brW - 12.0f, r.Bottom() - 18.0f}, 10.0f, pal.tapeTick);
    }

    canvas.PopClip();
}

} // namespace

Vec2 AttitudeView::Measure(
    const AttitudeViewOptions& opts,
    UiSize /*size*/,
    ICanvas* /*canvas*/,
    const UiTheme& theme,
    float availableWidth
) {
    float defaultW = availableWidth > 0.0f ? availableWidth : theme.Scale(520.0f);
    if (opts.stretch == StretchMode::Fill && availableWidth > 0.0f && opts.width.has_value()) {
        defaultW = availableWidth * std::clamp(*opts.width, 0.1f, 1.0f);
    } else if (opts.width.has_value() && *opts.width > 0.0f) {
        defaultW = theme.Scale(*opts.width);
    }
    const float defaultH = (opts.height.has_value() && *opts.height > 0.0f)
        ? theme.Scale(*opts.height)
        : theme.Scale(420.0f);
    return {defaultW, defaultH};
}

AttitudeViewResponse AttitudeView::Run(
    Context& ctx,
    const AttitudeViewOptions& optsIn,
    std::source_location loc
) {
    AttitudeViewOptions opts = optsIn;
    Control::InheritElementSizing(ctx, opts.stretch, opts.width, opts.height);

    const Id id = ctx.MakeChildId(loc);
    const bool isDisabled = opts.disabled || ctx.IsDisabled();
    const UiSize effectiveSize = ctx.GetCurrentSize();
    const UiTheme& theme = ctx.GetTheme();

    Response baseRes = ctx.Interact(id);
    AttitudeViewResponse resp{};
    static_cast<Response&>(resp) = baseRes;
    resp.displayMode = opts.displayMode;

    Rect prevBounds = baseRes.rect;
    if (prevBounds.w <= 1.0f || prevBounds.h <= 1.0f) {
        prevBounds = ctx.GetParentPrevFrameBounds();
    }
    const Rect activeRect = prevBounds;

    auto& state = ctx.GetOrCreateState<AttitudeViewState>(id);
    if (!state.initialized) {
        state.fovPitchDeg = std::clamp(opts.fieldOfViewPitchDeg, 25.0f, 90.0f);
        state.initialized = true;
    }
    if (opts.fovPitchDegPtr) {
        state.fovPitchDeg = std::clamp(*opts.fovPitchDegPtr, 25.0f, 90.0f);
    } else {
        state.fovPitchDeg = std::clamp(opts.fieldOfViewPitchDeg, 25.0f, 90.0f);
    }

    // Resolve ADI sub-rectangle for accurate mouse-to-attitude mapping
    const bool isFullPfd = (opts.displayMode == AttitudeDisplayMode::FullPFD);
    const bool isCompact = (opts.displayMode == AttitudeDisplayMode::CompactADI);
    const float pad = 8.0f;
    const float fmaH = (opts.showFmaBar && !isCompact && activeRect.h > 220.0f) ? 26.0f : 0.0f;
    const float hdgH = (opts.showHeadingTape && !isCompact && activeRect.h > 220.0f) ? 30.0f : 0.0f;
    const float leftTapeW = (isFullPfd && opts.showSpeedAndAltTapes && activeRect.w > 300.0f)
        ? std::clamp(activeRect.w * 0.14f, 56.0f, 76.0f)
        : 0.0f;
    const float rightTapeW = (isFullPfd && opts.showSpeedAndAltTapes && activeRect.w > 300.0f)
        ? std::clamp(activeRect.w * 0.19f, 76.0f, 106.0f)
        : 0.0f;

    const float contentTop = activeRect.y + pad + (fmaH > 0.0f ? (fmaH + 6.0f) : 0.0f);
    const float contentBot = activeRect.Bottom() - pad - (hdgH > 0.0f ? (hdgH + 6.0f) : 0.0f);
    const float contentH = std::max(40.0f, contentBot - contentTop);
    const float adiLeft = activeRect.x + pad + (leftTapeW > 0.0f ? (leftTapeW + 8.0f) : 0.0f);
    const float adiRight = activeRect.Right() - pad - (rightTapeW > 0.0f ? (rightTapeW + 8.0f) : 0.0f);
    const float availAdiW = std::max(40.0f, adiRight - adiLeft);
    const float adiSide = std::min(availAdiW, contentH);

    resp.adiRect = Rect(
        adiLeft + (availAdiW - adiSide) * 0.5f,
        contentTop + (contentH - adiSide) * 0.5f,
        adiSide,
        adiSide
    );
    resp.adiCenter = resp.adiRect.Center();
    resp.adiRadius = adiSide * 0.5f;

    const Vec2 mousePos = ctx.GetMousePos();
    const bool hovered = !isDisabled && activeRect.Contains(mousePos);
    resp.hovered = hovered;
    resp.cursorInside = hovered;

    if (hovered) {
        const Vec2 wheel = ctx.GetMouseWheel();
        if (std::abs(wheel.y) > 0.01f) {
            state.fovPitchDeg = std::clamp(state.fovPitchDeg - wheel.y * 4.0f, 25.0f, 90.0f);
            if (opts.fovPitchDegPtr) {
                *opts.fovPitchDegPtr = state.fovPitchDeg;
            }
            ctx.ConsumeMouseWheel();
            ctx.RequestRepaint(2);
            resp.changed = true;
        }

        const float invR = (resp.adiRadius > 1.0f) ? (1.0f / resp.adiRadius) : 0.0f;
        const float nx = std::clamp((mousePos.x - resp.adiCenter.x) * invR, -1.0f, 1.0f);
        const float ny = std::clamp((mousePos.y - resp.adiCenter.y) * invR, -1.0f, 1.0f);
        resp.cursorNorm = {nx, ny};
        resp.cursorInsideAdi = resp.adiRect.Contains(mousePos);
        resp.cursorRollDeg = nx * 65.0f;
        resp.cursorPitchDeg = -ny * 40.0f;
    }

    resp.visibleFovPitchDeg = state.fovPitchDeg;

    if (hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, baseRes.rect);
    }

    Node* node = ctx.CreateNode(NodeType::AttitudeView, id);
    auto* payload = ctx.AllocPayload<AttitudeViewPayload>();
    payload->opts = opts;
    payload->opts.tooltip = ctx.AllocString(opts.tooltip);
    payload->resp = resp;

    node->text = "AttitudeView";
    node->SetPayload(payload);

    node->visualHash = detail::HashArgs(
        opts.background,
        opts.border,
        static_cast<uint8_t>(opts.displayMode),
        static_cast<uint8_t>(opts.referenceMode),
        static_cast<uint8_t>(opts.colorScheme),
        static_cast<uint8_t>(opts.flightDirectorStyle),
        static_cast<uint8_t>(opts.aircraftSymbol),
        opts.rollDeg,
        opts.pitchDeg,
        opts.headingDeg,
        opts.airspeedKnots,
        opts.machNumber,
        opts.altitudeFeet,
        opts.verticalSpeedFpm,
        opts.slipSkid,
        opts.gLoad,
        opts.radioAltitudeFeet,
        opts.showFlightPathVector,
        opts.aoaDeg,
        opts.sideslipDeg,
        opts.fdPitchDeg,
        opts.fdRollDeg,
        opts.targetAirspeedBug,
        opts.targetAltitudeBug,
        opts.targetHeadingBug,
        opts.showIlsDiamonds,
        opts.locDeviationDots,
        opts.gsDeviationDots,
        opts.showPitchLadder,
        opts.showBankScale,
        opts.showSpeedAndAltTapes,
        opts.showHeadingTape,
        opts.showFmaBar,
        opts.showMouseStickOverlay,
        state.fovPitchDeg,
        resp.cursorInsideAdi ? resp.cursorNorm.x : -2.0f,
        resp.cursorInsideAdi ? resp.cursorNorm.y : -2.0f,
        activeRect.w,
        activeRect.h,
        effectiveSize,
        isDisabled
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* c, const UiTheme& th) noexcept -> Vec2 {
        const auto* p = n->As<AttitudeViewPayload>();
        if (!p) return {0.0f, 0.0f};
        return AttitudeView::Measure(p->opts, n->uiSize, c, th, availableWidth);
    };

    node->renderFn = &RenderAttitudeViewNode;

    node->uiSize = effectiveSize;
    node->disabled = isDisabled;
    node->hovered = hovered;
    node->pressed = baseRes.pressed;
    node->heightMode = opts.heightMode;

    ControlSpec ctrlSpec{};
    ctrlSpec.width = opts.width;
    ctrlSpec.height = opts.height;
    Control::SetupNode(ctx, node, ctrlSpec, opts.stretch, opts.align);
    if (!node->parent || node->parent->type != NodeType::Element) {
        node->heightMode = opts.heightMode;
        if (opts.height.has_value() && *opts.height > 0.0f) {
            node->heightMode = SizeMode::Fixed;
            node->fixedHeight = theme.Scale(*opts.height);
        }
        node->minHeight = (opts.height.has_value() && *opts.height > 0.0f) ? theme.Scale(*opts.height) : theme.Scale(260.0f);
        ctx.AdaptChildToContainer(node);
        if (opts.heightMode == SizeMode::Fit && !opts.height.has_value() && node->heightMode == SizeMode::Stretch) {
            node->heightMode = SizeMode::Fit;
        }
    }

    return resp;
}

} // namespace guikit
