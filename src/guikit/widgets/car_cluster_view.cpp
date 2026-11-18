#include "guikit/widgets/car_cluster_view.hpp"
#include <cmath>
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "keel/core.h"
#include "keel/format.h"
#include "keel/math.h"
#include "keel/ref.h"

namespace guikit {

namespace {

constexpr float kKmhToMph = 0.62137119f;
constexpr float kBarToPsi = 14.5037738f;

struct CarClusterViewState {
    float lastSpeedKmh{0.0f};
    double roadScrollPhase{0.0};
};

struct CarClusterViewPayload {
    CarClusterViewOptions opts{};
    CarClusterViewResponse resp{};
    float roadPhase{0.0f};
};

struct ClusterPalette {
    Color binnacleBg{};
    Color binnacleBorder{};
    Color dialBg{};
    Color dialInnerBg{};
    Color dialRim{};
    Color cardBg{};
    Color cardBorder{};
    Color primaryAccent{};
    Color secondaryAccent{};
    Color ecoGreen{};
    Color warningAmber{};
    Color dangerRed{};
    Color textBright{};
    Color textSecondary{};
    Color textMuted{};
    Color tickMajor{};
    Color tickMinor{};
};

[[nodiscard]] ClusterPalette ResolveClusterPalette(ClusterDriveTheme mode) noexcept {
    ClusterPalette p{};
    p.binnacleBg     = Color(7, 10, 17, 255);
    p.binnacleBorder = Color(38, 52, 74, 220);
    p.dialBg         = Color(11, 16, 27, 255);
    p.dialInnerBg    = Color(8, 12, 21, 255);
    p.dialRim        = Color(45, 62, 88, 180);
    p.cardBg         = Color(13, 19, 32, 235);
    p.cardBorder     = Color(42, 58, 82, 165);
    p.ecoGreen       = Color(16, 185, 129, 255);
    p.warningAmber   = Color(245, 158, 11, 255);
    p.dangerRed      = Color(244, 63, 94, 255);
    p.textBright     = Color(248, 250, 252, 255);
    p.textSecondary  = Color(182, 196, 216, 240);
    p.textMuted      = Color(116, 134, 160, 215);
    p.tickMajor      = Color(226, 232, 240, 235);
    p.tickMinor      = Color(100, 116, 139, 160);

    switch (mode) {
        case ClusterDriveTheme::ComfortCyan:
            p.primaryAccent   = Color(34, 211, 238, 255);
            p.secondaryAccent = Color(56, 189, 248, 255);
            break;
        case ClusterDriveTheme::SportCrimson:
            p.binnacleBg      = Color(8, 8, 12, 255);
            p.dialBg          = Color(15, 12, 18, 255);
            p.dialInnerBg     = Color(10, 8, 13, 255);
            p.cardBg          = Color(17, 14, 21, 235);
            p.binnacleBorder  = Color(88, 28, 42, 210);
            p.dialRim         = Color(96, 32, 46, 175);
            p.primaryAccent   = Color(244, 63, 94, 255);
            p.secondaryAccent = Color(251, 146, 60, 255);
            break;
        case ClusterDriveTheme::EcoEmerald:
            p.binnacleBg      = Color(6, 13, 15, 255);
            p.dialBg          = Color(9, 20, 23, 255);
            p.dialInnerBg     = Color(6, 14, 17, 255);
            p.cardBg          = Color(11, 23, 27, 235);
            p.binnacleBorder  = Color(24, 78, 68, 210);
            p.dialRim         = Color(30, 92, 80, 175);
            p.primaryAccent   = Color(16, 185, 129, 255);
            p.secondaryAccent = Color(45, 212, 191, 255);
            break;
        case ClusterDriveTheme::TrackGold:
            p.binnacleBg      = Color(10, 9, 8, 255);
            p.dialBg          = Color(17, 15, 12, 255);
            p.dialInnerBg     = Color(12, 10, 8, 255);
            p.cardBg          = Color(19, 17, 14, 235);
            p.binnacleBorder  = Color(92, 72, 24, 210);
            p.dialRim         = Color(105, 82, 28, 175);
            p.primaryAccent   = Color(250, 204, 21, 255);
            p.secondaryAccent = Color(245, 158, 11, 255);
            break;
    }
    return p;
}

[[nodiscard]] float DisplaySpeed(float speedKmh, SpeedUnit unit) noexcept {
    return (unit == SpeedUnit::Mph) ? (speedKmh * kKmhToMph) : speedKmh;
}

[[nodiscard]] float DisplayDistance(float distKm, SpeedUnit unit) noexcept {
    return (unit == SpeedUnit::Mph) ? (distKm * kKmhToMph) : distKm;
}

[[nodiscard]] float DisplayTemp(float tempC, SpeedUnit unit) noexcept {
    return (unit == SpeedUnit::Mph) ? (tempC * 1.8f + 32.0f) : tempC;
}

[[nodiscard]] float DisplayPressure(float bar, SpeedUnit unit) noexcept {
    return (unit == SpeedUnit::Mph) ? (bar * kBarToPsi) : bar;
}

[[nodiscard]] const char* SpeedUnitLabel(SpeedUnit unit) noexcept {
    return (unit == SpeedUnit::Mph) ? "mph" : "km/h";
}

[[nodiscard]] const char* DistUnitLabel(SpeedUnit unit) noexcept {
    return (unit == SpeedUnit::Mph) ? "mi" : "km";
}

[[nodiscard]] const char* TempUnitLabel(SpeedUnit unit) noexcept {
    return (unit == SpeedUnit::Mph) ? "F" : "C";
}

void DrawCircleRing(
    ICanvas& canvas,
    Vec2 center,
    float radius,
    Color fill,
    Color stroke,
    float strokeWidth = 1.0f
) noexcept {
    if (radius <= 0.5f) return;
    const Rect r(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);
    canvas.DrawRoundRect(r, radius, fill, stroke, strokeWidth);
}

/// Draws a circular arc from `startDeg` to `endDeg` where 0 deg = 3 o'clock (East), 90 deg = 6 o'clock (South),
/// 135 deg = 7:30 o'clock (bottom-left), 270 deg = 12 o'clock (North), 405 deg = 4:30 o'clock (bottom-right).
void DrawCircularArc(
    ICanvas& canvas,
    Vec2 center,
    float radius,
    float startDeg,
    float endDeg,
    Color color,
    float thickness,
    keel::i32 segments = 36
) noexcept {
    if (radius <= 1.0f || std::abs(endDeg - startDeg) < 0.2f || segments < 2) return;
    const float step = (endDeg - startDeg) / static_cast<float>(segments);
    Vec2 prev{
        center.x + std::cos(keel::deg_to_rad(startDeg)) * radius,
        center.y + std::sin(keel::deg_to_rad(startDeg)) * radius
    };
    for (keel::i32 i = 1; i <= segments; ++i) {
        const float deg = startDeg + step * static_cast<float>(i);
        const Vec2 cur{
            center.x + std::cos(keel::deg_to_rad(deg)) * radius,
            center.y + std::sin(keel::deg_to_rad(deg)) * radius
        };
        canvas.DrawLine(prev, cur, color, thickness);
        prev = cur;
    }
}

/// Draws a glowing tapered instrument needle from `center` toward `angleDeg`.
void DrawGaugeNeedle(
    ICanvas& canvas,
    Vec2 center,
    float angleDeg,
    float innerR,
    float outerR,
    float baseHalfWidth,
    Color needleColor,
    Color glowColor,
    float s
) noexcept {
    const float rad = keel::deg_to_rad(angleDeg);
    const Vec2 dir{std::cos(rad), std::sin(rad)};
    const Vec2 nrm{-dir.y, dir.x};

    const Vec2 tip{center.x + dir.x * outerR, center.y + dir.y * outerR};
    const Vec2 baseCenter{center.x + dir.x * innerR, center.y + dir.y * innerR};
    const Vec2 baseL{baseCenter.x + nrm.x * baseHalfWidth, baseCenter.y + nrm.y * baseHalfWidth};
    const Vec2 baseR{baseCenter.x - nrm.x * baseHalfWidth, baseCenter.y - nrm.y * baseHalfWidth};

    // Soft glow halo behind needle
    const Vec2 glowL{baseCenter.x + nrm.x * (baseHalfWidth * 2.2f), baseCenter.y + nrm.y * (baseHalfWidth * 2.2f)};
    const Vec2 glowR{baseCenter.x - nrm.x * (baseHalfWidth * 2.2f), baseCenter.y - nrm.y * (baseHalfWidth * 2.2f)};
    canvas.DrawTriangle(glowL, glowR, tip, glowColor.WithAlpha(55));

    // Solid core needle
    canvas.DrawTriangle(baseL, baseR, tip, needleColor);
    canvas.DrawLine(baseCenter, tip, Color(255, 255, 255, 210), 1.2f * s);
}

void FormatGearText(GearSelector sel, keel::i32 gear, char outMode[4], char outGear[8]) noexcept {
    const keel::i32 clampedGear = keel::clamp(gear, 1, 8);
    switch (sel) {
        case GearSelector::Park:
            keel::format_buf(outMode, 4, "P");
            keel::format_buf(outGear, 8, "PARK");
            break;
        case GearSelector::Reverse:
            keel::format_buf(outMode, 4, "R");
            keel::format_buf(outGear, 8, "REV");
            break;
        case GearSelector::Neutral:
            keel::format_buf(outMode, 4, "N");
            keel::format_buf(outGear, 8, "NEUT");
            break;
        case GearSelector::Drive:
            keel::format_buf(outMode, 4, "D");
            keel::format_buf(outGear, 8, "D%d", clampedGear);
            break;
        case GearSelector::Sport:
            keel::format_buf(outMode, 4, "S");
            keel::format_buf(outGear, 8, "S%d", clampedGear);
            break;
        case GearSelector::Manual:
            keel::format_buf(outMode, 4, "M");
            keel::format_buf(outGear, 8, "M%d", clampedGear);
            break;
    }
}

// ============================================================================
// 1. Sequential Shift-Light LED Strip (Top Center)
// ============================================================================
void RenderShiftLightStrip(
    ICanvas& canvas,
    const Rect& stripRect,
    const CarClusterViewOptions& opts,
    const ClusterPalette& pal,
    float s
) noexcept {
    if (stripRect.w < 80.0f || stripRect.h < 6.0f) return;

    float loadNorm = 0.0f;
    if (opts.powertrain == PowertrainMode::IceTurbo) {
        const float shiftStartRpm = keel::max(2500.0f, opts.redlineRpm * 0.58f);
        loadNorm = keel::clamp((opts.rpm - shiftStartRpm) / keel::max(500.0f, opts.redlineRpm - shiftStartRpm), 0.0f, 1.08f);
    } else {
        loadNorm = keel::clamp(opts.evPowerKw / 420.0f, 0.0f, 1.08f);
    }

    constexpr keel::i32 kNumLeds = 15;
    const float padX = 12.0f * s;
    const float availW = keel::max(40.0f, stripRect.w - padX * 2.0f);
    const float gap = 4.0f * s;
    const float ledW = keel::max(4.0f, (availW - gap * static_cast<float>(kNumLeds - 1)) / static_cast<float>(kNumLeds));
    const float ledH = keel::min(stripRect.h - 2.0f * s, 7.0f * s);
    const float y = stripRect.y + (stripRect.h - ledH) * 0.5f;

    const bool shiftFlash = (loadNorm >= 0.97f);
    const keel::i32 litCount = keel::round_i32(keel::clamp(loadNorm, 0.0f, 1.0f) * static_cast<float>(kNumLeds));

    for (keel::i32 i = 0; i < kNumLeds; ++i) {
        const float x = stripRect.x + padX + static_cast<float>(i) * (ledW + gap);
        const Rect lr(x, y, ledW, ledH);
        const bool isLit = (i < litCount) && (!shiftFlash || opts.telltales.blinkPhaseOn);

        Color ledCol = Color(24, 34, 48, 190);
        if (isLit) {
            if (shiftFlash) {
                ledCol = (i % 2 == 0) ? Color(244, 63, 94, 255) : Color(56, 189, 248, 255);
            } else if (i < 6) {
                ledCol = Color(34, 197, 94, 255);
            } else if (i < 11) {
                ledCol = Color(250, 204, 21, 255);
            } else {
                ledCol = Color(244, 63, 94, 255);
            }
        }

        canvas.DrawRoundRect(lr, ledH * 0.45f, ledCol, isLit ? Color(255, 255, 255, 90) : pal.cardBorder, 0.8f * s);
    }
}

// ============================================================================
// 2. ISO Automotive Telltale Warning & Status Ribbon
// ============================================================================
void DrawTurnSignalArrow(ICanvas& canvas, Rect box, bool isLeft, bool active, float s) noexcept {
    const Color fill = active ? Color(34, 197, 94, 255) : Color(30, 41, 59, 170);
    const Color stroke = active ? Color(134, 239, 172, 255) : Color(71, 85, 105, 150);
    const Vec2 c = box.Center();
    const float w = keel::min(box.w, box.h) * 0.44f;
    const float h = w * 0.82f;

    if (isLeft) {
        canvas.DrawTriangle({c.x - w, c.y}, {c.x, c.y - h}, {c.x, c.y + h}, fill, stroke, 1.0f * s);
        canvas.DrawRect(Rect(c.x, c.y - h * 0.45f, w * 0.78f, h * 0.90f), fill);
    } else {
        canvas.DrawTriangle({c.x + w, c.y}, {c.x, c.y - h}, {c.x, c.y + h}, fill, stroke, 1.0f * s);
        canvas.DrawRect(Rect(c.x - w * 0.78f, c.y - h * 0.45f, w * 0.78f, h * 0.90f), fill);
    }
}

void DrawTelltalePill(
    ICanvas& canvas,
    Rect r,
    keel::StringRef label,
    bool active,
    Color activeColor,
    float s
) noexcept {
    const Color bg = active ? activeColor.WithAlpha(38) : Color(18, 25, 38, 160);
    const Color border = active ? activeColor : Color(51, 65, 85, 120);
    const Color textCol = active ? activeColor : Color(100, 116, 139, 145);
    canvas.DrawRoundRect(r, 4.0f * s, bg, border, (active ? 1.2f : 0.8f) * s);
    canvas.DrawTextBox(label, r, 9.5f * s, textCol, 0.5f, 0.5f);
}

void RenderTelltaleBar(
    ICanvas& canvas,
    const Rect& barRect,
    const CarClusterViewOptions& opts,
    const ClusterPalette& pal,
    float s
) noexcept {
    if (barRect.w < 160.0f || barRect.h < 18.0f) return;

    const auto& tt = opts.telltales;
    const bool leftBlink = (tt.leftTurn || tt.hazard) && tt.blinkPhaseOn;
    const bool rightBlink = (tt.rightTurn || tt.hazard) && tt.blinkPhaseOn;

    const float arrowSize = keel::min(barRect.h - 2.0f * s, 22.0f * s);
    const float cy = barRect.y + barRect.h * 0.5f;

    // Left Turn Arrow
    const Rect leftArrowRect(barRect.x + 8.0f * s, cy - arrowSize * 0.5f, arrowSize * 1.2f, arrowSize);
    DrawTurnSignalArrow(canvas, leftArrowRect, true, leftBlink, s);

    // Right Turn Arrow
    const Rect rightArrowRect(barRect.Right() - 8.0f * s - arrowSize * 1.2f, cy - arrowSize * 0.5f, arrowSize * 1.2f, arrowSize);
    DrawTurnSignalArrow(canvas, rightArrowRect, false, rightBlink, s);

    // Left cluster of lighting & chassis telltale badges
    const float pillH = keel::min(barRect.h - 4.0f * s, 18.0f * s);
    const float pillY = cy - pillH * 0.5f;
    float lx = leftArrowRect.Right() + 8.0f * s;
    const float gap = 5.0f * s;

    DrawTelltalePill(canvas, Rect(lx, pillY, 36.0f * s, pillH), "LOW", tt.lowBeam, Color(34, 197, 94, 255), s);
    lx += 36.0f * s + gap;
    DrawTelltalePill(canvas, Rect(lx, pillY, 40.0f * s, pillH), "HIGH", tt.highBeam, Color(59, 130, 246, 255), s);
    lx += 40.0f * s + gap;
    if (barRect.w > 540.0f) {
        DrawTelltalePill(canvas, Rect(lx, pillY, 36.0f * s, pillH), "FOG", tt.fogLight, pal.warningAmber, s);
        lx += 36.0f * s + gap;
        DrawTelltalePill(canvas, Rect(lx, pillY, 38.0f * s, pillH), "LKA", tt.laneKeepActive, pal.ecoGreen, s);
    }

    // Right cluster of safety & warning telltale badges
    float rx = rightArrowRect.x - 8.0f * s;
    auto pushRightPill = [&](keel::StringRef text, float w, bool on, Color c) {
        rx -= w;
        if (rx > lx + 80.0f * s) {
            DrawTelltalePill(canvas, Rect(rx, pillY, w, pillH), text, on, c, s);
        }
        rx -= gap;
    };

    pushRightPill("PARK",  42.0f * s, tt.parkBrake || opts.gearSelector == GearSelector::Park, pal.dangerRed);
    pushRightPill("BELT",  40.0f * s, tt.seatbeltWarning, pal.dangerRed);
    pushRightPill((opts.powertrain == PowertrainMode::IceTurbo) ? "CHECK" : "BATT", 44.0f * s, tt.batteryOrEngineAlert, pal.dangerRed);
    pushRightPill("ESC",   36.0f * s, tt.escActive, pal.warningAmber);
    if (barRect.w > 540.0f) {
        pushRightPill("ABS",  36.0f * s, tt.absWarning, pal.warningAmber);
        pushRightPill("TPMS", 42.0f * s, tt.tpmsWarning, pal.warningAmber);
    }

    // Center Drive Mode & Powertrain Status Pill
    const char* themeTitle = "COMFORT";
    switch (opts.driveTheme) {
        case ClusterDriveTheme::ComfortCyan:  themeTitle = "COMFORT"; break;
        case ClusterDriveTheme::SportCrimson: themeTitle = "SPORT+";  break;
        case ClusterDriveTheme::EcoEmerald:   themeTitle = "ECO PRO"; break;
        case ClusterDriveTheme::TrackGold:    themeTitle = "TRACK";   break;
    }
    char centerBadge[64]{};
    const keel::StringRef centerBadgeRef = keel::format_buf(
        centerBadge,
        "%s  ·  %s",
        themeTitle,
        (opts.powertrain == PowertrainMode::IceTurbo) ? "TWIN-TURBO 8-DCT" : "DUAL-MOTOR AWD"
    );
    const float centerW = keel::min(220.0f * s, keel::max(100.0f * s, rx - lx - 12.0f * s));
    if (centerW >= 100.0f * s) {
        const Rect centerPill(barRect.Center().x - centerW * 0.5f, pillY, centerW, pillH);
        canvas.DrawRoundRect(centerPill, pillH * 0.5f, pal.primaryAccent.WithAlpha(28), pal.primaryAccent.WithAlpha(185), 1.0f * s);
        canvas.DrawTextBox(centerBadgeRef, centerPill, 10.0f * s, pal.textBright, 0.5f, 0.5f);
    }
}

// ============================================================================
// 3. Precision Circular Speedometer Dial
// ============================================================================
void RenderSpeedometerDial(
    ICanvas& canvas,
    const Rect& dialBox,
    const CarClusterViewOptions& opts,
    const ClusterPalette& pal,
    float s
) noexcept {
    const Vec2 center = dialBox.Center();
    const float radius = keel::min(dialBox.w, dialBox.h) * 0.47f;
    if (radius < 45.0f) return;

    // Outer metallic ring & recessed dial face
    DrawCircleRing(canvas, center, radius, pal.dialBg, pal.dialRim, 2.0f * s);
    DrawCircleRing(canvas, center, radius * 0.93f, pal.dialInnerBg, pal.cardBorder, 1.0f * s);

    // Scale parameters: 135 deg (7:30) to 405 deg (4:30) -> 270 deg sweep
    constexpr float kStartDeg = 135.0f;
    constexpr float kSweepDeg = 270.0f;
    const float maxSpdDisp = keel::max(60.0f, DisplaySpeed(opts.maxSpeedKmh, opts.speedUnit));
    const float curSpdDisp = keel::clamp(DisplaySpeed(opts.speedKmh, opts.speedUnit), 0.0f, maxSpdDisp);
    const float spdNorm = keel::clamp(curSpdDisp / maxSpdDisp, 0.0f, 1.0f);
    const float needleDeg = kStartDeg + spdNorm * kSweepDeg;

    // Background track arc & glowing active speed arc
    const float arcR = radius * 0.86f;
    DrawCircularArc(canvas, center, arcR, kStartDeg, kStartDeg + kSweepDeg, Color(30, 41, 59, 165), 5.0f * s, 44);
    if (spdNorm > 0.004f) {
        DrawCircularArc(canvas, center, arcR, kStartDeg, needleDeg, pal.primaryAccent.WithAlpha(90), 8.5f * s, 36);
        DrawCircularArc(canvas, center, arcR, kStartDeg, needleDeg, pal.primaryAccent, 4.2f * s, 36);
    }

    // Adaptive Cruise Control (ACC) target bug on the rim
    if (opts.cruiseSet && opts.telltales.adasCruiseActive) {
        const float cruiseDisp = keel::clamp(DisplaySpeed(opts.cruiseTargetKmh, opts.speedUnit), 0.0f, maxSpdDisp);
        const float cruiseDeg = kStartDeg + (cruiseDisp / maxSpdDisp) * kSweepDeg;
        const float cRad = keel::deg_to_rad(cruiseDeg);
        const Vec2 bugTip{center.x + std::cos(cRad) * (arcR - 4.0f * s), center.y + std::sin(cRad) * (arcR - 4.0f * s)};
        const Vec2 bugL{
            center.x + std::cos(keel::deg_to_rad(cruiseDeg - 3.2f)) * (arcR + 6.0f * s),
            center.y + std::sin(keel::deg_to_rad(cruiseDeg - 3.2f)) * (arcR + 6.0f * s)
        };
        const Vec2 bugR{
            center.x + std::cos(keel::deg_to_rad(cruiseDeg + 3.2f)) * (arcR + 6.0f * s),
            center.y + std::sin(keel::deg_to_rad(cruiseDeg + 3.2f)) * (arcR + 6.0f * s)
        };
        canvas.DrawTriangle(bugTip, bugL, bugR, pal.ecoGreen);
    }

    // Major & Minor Speed Ticks + Numeric Labels
    const keel::i32 majorStep = (opts.speedUnit == SpeedUnit::Mph) ? 20 : 30;
    const keel::i32 minorStep = (opts.speedUnit == SpeedUnit::Mph) ? 5 : 10;
    const keel::i32 maxTick = keel::round_i32(maxSpdDisp);

    for (keel::i32 v = 0; v <= maxTick; v += minorStep) {
        const float frac = static_cast<float>(v) / maxSpdDisp;
        if (frac > 1.001f) break;
        const float deg = kStartDeg + frac * kSweepDeg;
        const float rad = keel::deg_to_rad(deg);
        const bool isMajor = (v % majorStep == 0);
        const float rOuter = radius * 0.89f;
        const float rInner = radius * (isMajor ? 0.78f : 0.835f);

        const Vec2 p0{center.x + std::cos(rad) * rInner, center.y + std::sin(rad) * rInner};
        const Vec2 p1{center.x + std::cos(rad) * rOuter, center.y + std::sin(rad) * rOuter};
        const Color tickCol = (static_cast<float>(v) <= curSpdDisp + 0.5f)
            ? (isMajor ? pal.textBright : pal.primaryAccent)
            : (isMajor ? pal.tickMajor : pal.tickMinor);
        canvas.DrawLine(p0, p1, tickCol, (isMajor ? 2.0f : 1.0f) * s);

        if (isMajor && radius >= 68.0f) {
            const float labelR = radius * 0.65f;
            const Vec2 lp{center.x + std::cos(rad) * labelR, center.y + std::sin(rad) * labelR};
            char buf[16]{};
            const Rect lblBox(lp.x - 16.0f * s, lp.y - 8.0f * s, 32.0f * s, 16.0f * s);
            canvas.DrawTextBox(keel::format_buf(buf, "%d", v), lblBox, keel::clamp(radius * 0.095f, 8.5f * s, 12.5f * s),
                               (static_cast<float>(v) <= curSpdDisp + 0.5f) ? pal.textBright : pal.textSecondary, 0.5f, 0.5f);
        }
    }

    // Needle + Center Hub
    DrawGaugeNeedle(canvas, center, needleDeg, radius * 0.30f, radius * 0.85f, 3.2f * s, pal.primaryAccent, pal.primaryAccent, s);
    DrawCircleRing(canvas, center, radius * 0.38f, pal.dialInnerBg, pal.dialRim, 1.5f * s);

    // Center Digital Speed Readout
    char spdBuf[16]{};
    const float bigFs = keel::clamp(radius * 0.34f, 18.0f * s, 38.0f * s);
    const float subFs = keel::clamp(radius * 0.095f, 8.5f * s, 12.0f * s);

    canvas.DrawTextBox(
        keel::format_buf(spdBuf, "%.0f", curSpdDisp),
        Rect(center.x - radius * 0.36f, center.y - bigFs * 0.68f, radius * 0.72f, bigFs * 1.15f),
        bigFs,
        pal.textBright,
        0.5f,
        0.5f
    );
    canvas.DrawTextBox(
        SpeedUnitLabel(opts.speedUnit),
        Rect(center.x - radius * 0.30f, center.y + bigFs * 0.34f, radius * 0.60f, subFs * 1.4f),
        subFs,
        pal.primaryAccent,
        0.5f,
        0.5f
    );

    // Bottom Opening (55 deg .. 125 deg): Fuel / SoC Bar & Range Readout
    const float fuelNorm = keel::clamp(opts.fuelOrSocPct * 0.01f, 0.0f, 1.0f);
    const Color fuelCol = (fuelNorm < 0.15f) ? pal.dangerRed : ((opts.powertrain == PowertrainMode::ElectricDualMotor) ? pal.ecoGreen : pal.secondaryAccent);
    const float botArcR = radius * 0.76f;
    DrawCircularArc(canvas, center, botArcR, 122.0f, 58.0f, Color(30, 41, 59, 170), 4.0f * s, 20);
    if (fuelNorm > 0.01f) {
        DrawCircularArc(canvas, center, botArcR, 122.0f, 122.0f - fuelNorm * 64.0f, fuelCol, 4.2f * s, 20);
    }

    char fuelBuf[32]{};
    const float dispRange = DisplayDistance(opts.estimatedRangeKm, opts.speedUnit);
    const keel::StringRef fuelRef = keel::format_buf(
        fuelBuf,
        "%s %.0f%% · %.0f %s",
        (opts.powertrain == PowertrainMode::ElectricDualMotor) ? "SOC" : "FUEL",
        opts.fuelOrSocPct,
        dispRange,
        DistUnitLabel(opts.speedUnit)
    );
    canvas.DrawTextBox(
        fuelRef,
        Rect(center.x - radius * 0.56f, center.y + radius * 0.50f, radius * 1.12f, 16.0f * s),
        keel::clamp(radius * 0.088f, 8.0f * s, 11.0f * s),
        pal.textSecondary,
        0.5f,
        0.5f
    );
}

// ============================================================================
// 4. Precision Circular Tachometer (ICE) or Power/Regen Dial (EV)
// ============================================================================
void RenderTachOrPowerDial(
    ICanvas& canvas,
    const Rect& dialBox,
    const CarClusterViewOptions& opts,
    const ClusterPalette& pal,
    bool showSpeedInsideCenter,
    float s
) noexcept {
    const Vec2 center = dialBox.Center();
    const float radius = keel::min(dialBox.w, dialBox.h) * 0.47f;
    if (radius < 45.0f) return;

    DrawCircleRing(canvas, center, radius, pal.dialBg, pal.dialRim, 2.0f * s);
    DrawCircleRing(canvas, center, radius * 0.93f, pal.dialInnerBg, pal.cardBorder, 1.0f * s);

    constexpr float kStartDeg = 135.0f;
    constexpr float kSweepDeg = 270.0f;
    const float arcR = radius * 0.86f;

    float needleDeg = kStartDeg;
    Color activeArcCol = pal.primaryAccent;

    if (opts.powertrain == PowertrainMode::IceTurbo) {
        const float maxRpm = keel::max(6000.0f, opts.maxRpm);
        const float redlineRpm = keel::clamp(opts.redlineRpm, 4500.0f, maxRpm);
        const float curRpm = keel::clamp(opts.rpm, 0.0f, maxRpm);
        const float rpmNorm = curRpm / maxRpm;
        const float redlineNorm = redlineRpm / maxRpm;
        needleDeg = kStartDeg + rpmNorm * kSweepDeg;
        const float redlineStartDeg = kStartDeg + redlineNorm * kSweepDeg;

        // Track & Redline arc
        DrawCircularArc(canvas, center, arcR, kStartDeg, redlineStartDeg, Color(30, 41, 59, 165), 5.0f * s, 36);
        DrawCircularArc(canvas, center, arcR, redlineStartDeg, kStartDeg + kSweepDeg, pal.dangerRed.WithAlpha(115), 5.5f * s, 16);

        activeArcCol = (curRpm >= redlineRpm) ? pal.dangerRed : pal.primaryAccent;
        if (rpmNorm > 0.004f) {
            DrawCircularArc(canvas, center, arcR, kStartDeg, needleDeg, activeArcCol.WithAlpha(90), 8.5f * s, 36);
            DrawCircularArc(canvas, center, arcR, kStartDeg, needleDeg, activeArcCol, 4.2f * s, 36);
        }

        // Ticks: 0..8 (or 9) x1000 RPM
        const keel::i32 maxK = keel::ceil_i32(maxRpm / 1000.0f);
        const keel::i32 totalSubTicks = maxK * 5;
        for (keel::i32 t = 0; t <= totalSubTicks; ++t) {
            const float rpmVal = static_cast<float>(t) * 200.0f;
            if (rpmVal > maxRpm + 10.0f) break;
            const float frac = rpmVal / maxRpm;
            const float deg = kStartDeg + frac * kSweepDeg;
            const float rad = keel::deg_to_rad(deg);
            const bool isMajor = (t % 5 == 0);
            const bool inRed = (rpmVal >= redlineRpm - 10.0f);

            const float rOuter = radius * 0.89f;
            const float rInner = radius * (isMajor ? 0.78f : 0.835f);
            const Vec2 p0{center.x + std::cos(rad) * rInner, center.y + std::sin(rad) * rInner};
            const Vec2 p1{center.x + std::cos(rad) * rOuter, center.y + std::sin(rad) * rOuter};
            const Color tickCol = inRed ? pal.dangerRed : (isMajor ? pal.tickMajor : pal.tickMinor);
            canvas.DrawLine(p0, p1, tickCol, (isMajor ? 2.2f : 1.0f) * s);

            if (isMajor && radius >= 68.0f) {
                const float labelR = radius * 0.65f;
                const Vec2 lp{center.x + std::cos(rad) * labelR, center.y + std::sin(rad) * labelR};
                char buf[8]{};
                canvas.DrawTextBox(
                    keel::format_buf(buf, "%d", t / 5),
                    Rect(lp.x - 12.0f * s, lp.y - 8.0f * s, 24.0f * s, 16.0f * s),
                    keel::clamp(radius * 0.105f, 9.0f * s, 13.5f * s),
                    inRed ? pal.dangerRed : pal.textBright,
                    0.5f,
                    0.5f
                );
            }
        }
    } else {
        // Electric Dual-Motor Power / Regen Gauge (-150 kW Regen .. 0 .. +450 kW Power)
        // Zero at 202.5 deg (25% of 270 deg sweep)
        constexpr float kZeroDeg = kStartDeg + 67.5f; // 202.5 deg
        DrawCircularArc(canvas, center, arcR, kStartDeg, kZeroDeg, pal.ecoGreen.WithAlpha(75), 5.0f * s, 16);
        DrawCircularArc(canvas, center, arcR, kZeroDeg, kStartDeg + kSweepDeg, Color(30, 41, 59, 165), 5.0f * s, 32);

        const float kw = keel::clamp(opts.evPowerKw, -150.0f, 450.0f);
        if (kw >= 0.0f) {
            const float pNorm = kw / 450.0f;
            needleDeg = kZeroDeg + pNorm * (270.0f - 67.5f);
            activeArcCol = (pNorm > 0.82f) ? pal.dangerRed : pal.primaryAccent;
            if (pNorm > 0.005f) {
                DrawCircularArc(canvas, center, arcR, kZeroDeg, needleDeg, activeArcCol.WithAlpha(90), 8.5f * s, 28);
                DrawCircularArc(canvas, center, arcR, kZeroDeg, needleDeg, activeArcCol, 4.2f * s, 28);
            }
        } else {
            const float rNorm = (-kw) / 150.0f;
            needleDeg = kZeroDeg - rNorm * 67.5f;
            activeArcCol = pal.ecoGreen;
            if (rNorm > 0.005f) {
                DrawCircularArc(canvas, center, arcR, needleDeg, kZeroDeg, pal.ecoGreen.WithAlpha(90), 8.5f * s, 16);
                DrawCircularArc(canvas, center, arcR, needleDeg, kZeroDeg, pal.ecoGreen, 4.2f * s, 16);
            }
        }

        // EV scale labels: CHG, 0, 25, 50, 75, 100 % POWER
        struct EvTick { float frac; const char* label; Color col; };
        const EvTick kEvTicks[] = {
            {0.00f, "-30",  pal.ecoGreen},
            {0.25f, "0",    pal.textBright},
            {0.4375f, "25", pal.textSecondary},
            {0.625f, "50",  pal.textSecondary},
            {0.8125f, "75", pal.textSecondary},
            {1.00f, "100",  pal.dangerRed}
        };
        for (const auto& et : kEvTicks) {
            const float deg = kStartDeg + et.frac * kSweepDeg;
            const float rad = keel::deg_to_rad(deg);
            const Vec2 p0{center.x + std::cos(rad) * (radius * 0.78f), center.y + std::sin(rad) * (radius * 0.78f)};
            const Vec2 p1{center.x + std::cos(rad) * (radius * 0.89f), center.y + std::sin(rad) * (radius * 0.89f)};
            canvas.DrawLine(p0, p1, et.col, 2.0f * s);
            if (radius >= 68.0f) {
                const float labelR = radius * 0.64f;
                const Vec2 lp{center.x + std::cos(rad) * labelR, center.y + std::sin(rad) * labelR};
                canvas.DrawTextBox(
                    et.label,
                    Rect(lp.x - 16.0f * s, lp.y - 8.0f * s, 32.0f * s, 16.0f * s),
                    keel::clamp(radius * 0.092f, 8.5f * s, 11.5f * s),
                    et.col,
                    0.5f,
                    0.5f
                );
            }
        }
    }

    // Needle + Center Hub
    const float hubR = radius * (showSpeedInsideCenter ? 0.44f : 0.38f);
    DrawGaugeNeedle(canvas, center, needleDeg, hubR * 0.82f, radius * 0.85f, 3.2f * s, activeArcCol, activeArcCol, s);
    DrawCircleRing(canvas, center, hubR, pal.dialInnerBg, pal.dialRim, 1.5f * s);

    char modeStr[4]{};
    char gearStr[8]{};
    FormatGearText(opts.gearSelector, opts.currentGear, modeStr, gearStr);

    if (showSpeedInsideCenter) {
        // TrackCentral mode: Speed + Gear badge + RPM/kW inside the dominant central dial
        char spdBuf[16]{};
        const float bigFs = keel::clamp(radius * 0.32f, 20.0f * s, 40.0f * s);
        const float subFs = keel::clamp(radius * 0.090f, 8.5f * s, 11.5f * s);

        canvas.DrawTextBox(
            keel::format_buf(spdBuf, "%.0f", DisplaySpeed(opts.speedKmh, opts.speedUnit)),
            Rect(center.x - hubR * 0.85f, center.y - hubR * 0.65f, hubR * 1.70f, bigFs * 1.1f),
            bigFs,
            pal.textBright,
            0.5f,
            0.5f
        );
        canvas.DrawTextBox(
            SpeedUnitLabel(opts.speedUnit),
            Rect(center.x - hubR * 0.60f, center.y - hubR * 0.02f, hubR * 1.20f, subFs * 1.3f),
            subFs,
            pal.textMuted,
            0.5f,
            0.5f
        );

        const Rect gearBox(center.x - 24.0f * s, center.y + hubR * 0.22f, 48.0f * s, 20.0f * s);
        canvas.DrawRoundRect(gearBox, 4.0f * s, activeArcCol.WithAlpha(42), activeArcCol, 1.2f * s);
        canvas.DrawTextBox(gearStr, gearBox, 12.0f * s, pal.textBright, 0.5f, 0.5f);
    } else {
        // DualDial mode: Large Gear Indicator + Digital RPM or kW
        const float gearFs = keel::clamp(radius * 0.30f, 17.0f * s, 32.0f * s);
        const float subFs = keel::clamp(radius * 0.092f, 8.5f * s, 11.5f * s);

        canvas.DrawTextBox(
            gearStr,
            Rect(center.x - hubR * 0.80f, center.y - gearFs * 0.68f, hubR * 1.60f, gearFs * 1.15f),
            gearFs,
            activeArcCol,
            0.5f,
            0.5f
        );

        char subBuf[24]{};
        const keel::StringRef subRef = (opts.powertrain == PowertrainMode::IceTurbo)
            ? keel::format_buf(subBuf, "%.0f RPM", opts.rpm)
            : keel::format_buf(subBuf, "%+.0f kW", opts.evPowerKw);
        canvas.DrawTextBox(
            subRef,
            Rect(center.x - hubR * 0.88f, center.y + gearFs * 0.34f, hubR * 1.76f, subFs * 1.4f),
            subFs,
            pal.textSecondary,
            0.5f,
            0.5f
        );
    }

    // Bottom Opening: Coolant / Oil / Battery Temperature Bar
    const float tempC = opts.coolantOrBattTempC;
    const float tempNorm = keel::clamp((tempC - 40.0f) / 90.0f, 0.0f, 1.0f);
    const Color tempCol = (tempC > 112.0f) ? pal.dangerRed : pal.secondaryAccent;
    const float botArcR = radius * 0.76f;
    DrawCircularArc(canvas, center, botArcR, 122.0f, 58.0f, Color(30, 41, 59, 170), 4.0f * s, 20);
    if (tempNorm > 0.01f) {
        DrawCircularArc(canvas, center, botArcR, 122.0f, 122.0f - tempNorm * 64.0f, tempCol, 4.2f * s, 20);
    }

    char tempBuf[36]{};
    const keel::StringRef tempRef = (opts.powertrain == PowertrainMode::IceTurbo)
        ? keel::format_buf(
              tempBuf,
              "OIL %.0f%s · %+.1f bar",
              DisplayTemp(opts.oilTempC, opts.speedUnit),
              TempUnitLabel(opts.speedUnit),
              opts.boostBar
          )
        : keel::format_buf(
              tempBuf,
              "BATT %.0f%s · %.1f kWh",
              DisplayTemp(opts.coolantOrBattTempC, opts.speedUnit),
              TempUnitLabel(opts.speedUnit),
              opts.avgConsumption
          );
    canvas.DrawTextBox(
        tempRef,
        Rect(center.x - radius * 0.56f, center.y + radius * 0.50f, radius * 1.12f, 16.0f * s),
        keel::clamp(radius * 0.086f, 8.0f * s, 11.0f * s),
        pal.textSecondary,
        0.5f,
        0.5f
    );
}

// ============================================================================
// 5. Center MFD Viewport (ADAS 3D Lane, TPMS Chassis, G-Force Dynamics)
// ============================================================================
void RenderAdasLaneRadarPage(
    ICanvas& canvas,
    const Rect& bodyRect,
    const CarClusterViewOptions& opts,
    const ClusterPalette& pal,
    float roadPhase,
    float s
) noexcept {
    canvas.PushClip(bodyRect);

    const float horizonY = bodyRect.y + bodyRect.h * 0.26f;
    const float botY = bodyRect.Bottom() - 4.0f * s;
    const float roadH = keel::max(20.0f, botY - horizonY);
    const float cx = bodyRect.Center().x;

    // Sky & Horizon Glow
    canvas.DrawRect(Rect(bodyRect.x, bodyRect.y, bodyRect.w, horizonY - bodyRect.y), Color(9, 15, 28, 255));
    canvas.DrawRect(Rect(bodyRect.x, horizonY, bodyRect.w, bodyRect.Bottom() - horizonY), Color(12, 18, 30, 255));
    canvas.DrawLine({bodyRect.x, horizonY}, {bodyRect.Right(), horizonY}, pal.primaryAccent.WithAlpha(95), 1.2f * s);

    // Perspective 3D road curvature from steeringAngleDeg
    const float steerCurve = keel::clamp(opts.steeringAngleDeg / 45.0f, -1.0f, 1.0f) * (bodyRect.w * 0.22f);
    const float topHalfW = bodyRect.w * 0.08f;
    const float botHalfW = bodyRect.w * 0.44f;

    // Road surface polygon slices with steering curvature
    constexpr keel::i32 kRoadSlices = 14;
    auto laneCenterAt = [&](float depthT) noexcept -> float {
        // depthT: 0.0 at horizon (top), 1.0 at bottom (ego car)
        const float farFactor = (1.0f - depthT) * (1.0f - depthT);
        return cx + steerCurve * farFactor;
    };
    auto laneHalfWidthAt = [&](float depthT) noexcept -> float {
        return topHalfW + (botHalfW - topHalfW) * depthT;
    };

    const Color laneRailCol = opts.telltales.laneKeepActive ? pal.ecoGreen : pal.textMuted;
    const Color carpetCol   = pal.primaryAccent.WithAlpha(34);

    for (keel::i32 i = 0; i < kRoadSlices; ++i) {
        const float t0 = static_cast<float>(i) / static_cast<float>(kRoadSlices);
        const float t1 = static_cast<float>(i + 1) / static_cast<float>(kRoadSlices);
        const float y0 = horizonY + t0 * roadH;
        const float y1 = horizonY + t1 * roadH;

        const float cx0 = laneCenterAt(t0);
        const float cx1 = laneCenterAt(t1);
        const float hw0 = laneHalfWidthAt(t0);
        const float hw1 = laneHalfWidthAt(t1);

        // Ego lane guidance carpet (inner 66% of lane width)
        const Vec2 cL0{cx0 - hw0 * 0.66f, y0};
        const Vec2 cR0{cx0 + hw0 * 0.66f, y0};
        const Vec2 cL1{cx1 - hw1 * 0.66f, y1};
        const Vec2 cR1{cx1 + hw1 * 0.66f, y1};
        canvas.DrawTriangle(cL0, cR0, cR1, carpetCol);
        canvas.DrawTriangle(cL0, cR1, cL1, carpetCol);

        // Left & Right Lane Boundary Rails
        canvas.DrawLine({cx0 - hw0, y0}, {cx1 - hw1, y1}, laneRailCol, (1.2f + t1 * 1.6f) * s);
        canvas.DrawLine({cx0 + hw0, y0}, {cx1 + hw1, y1}, laneRailCol, (1.2f + t1 * 1.6f) * s);

        // Adjacent Left & Right Outer Highway Lanes (subtle)
        canvas.DrawLine({cx0 - hw0 * 2.2f, y0}, {cx1 - hw1 * 2.2f, y1}, Color(71, 85, 105, 85), 1.0f * s);
        canvas.DrawLine({cx0 + hw0 * 2.2f, y0}, {cx1 + hw1 * 2.2f, y1}, Color(71, 85, 105, 85), 1.0f * s);
    }

    // Animated scrolling distance chevrons along ego lane carpet
    for (keel::i32 k = 0; k < 5; ++k) {
        float depthT = std::fmod(static_cast<float>(k) * 0.20f + roadPhase, 1.0f);
        depthT = depthT * depthT; // perspective foreshortening
        if (depthT < 0.08f || depthT > 0.85f) continue;
        const float y = horizonY + depthT * roadH;
        const float lcx = laneCenterAt(depthT);
        const float hw = laneHalfWidthAt(depthT) * 0.42f;
        const Color chevCol = pal.primaryAccent.WithAlpha(static_cast<keel::u8>(50 + depthT * 120.0f));
        canvas.DrawLine({lcx - hw, y + 4.0f * s}, {lcx, y}, chevCol, 1.5f * s);
        canvas.DrawLine({lcx, y}, {lcx + hw, y + 4.0f * s}, chevCol, 1.5f * s);
    }

    // Radar Lead Vehicle (scaled by adasLeadDistanceM in [10 .. 100 m])
    if (opts.adasLeadDetected) {
        const float distM = keel::clamp(opts.adasLeadDistanceM, 8.0f, 110.0f);
        const float distNorm = 1.0f - keel::clamp((distM - 8.0f) / 102.0f, 0.0f, 1.0f);
        const float leadDepthT = 0.14f + distNorm * 0.46f;
        const float leadY = horizonY + leadDepthT * roadH;
        const float leadCx = laneCenterAt(leadDepthT);
        const float carW = keel::clamp(bodyRect.w * (0.10f + leadDepthT * 0.22f), 22.0f * s, 72.0f * s);
        const float carH = carW * 0.62f;

        const Color radarCol = (distM < 18.0f) ? pal.dangerRed : ((distM < 32.0f) ? pal.warningAmber : pal.ecoGreen);

        // Radar target lock bracket under lead car
        canvas.DrawLine({leadCx - carW * 0.62f, leadY + 2.0f * s}, {leadCx + carW * 0.62f, leadY + 2.0f * s}, radarCol, 2.2f * s);

        // Lead vehicle rear silhouette
        const Rect lowerBody(leadCx - carW * 0.5f, leadY - carH * 0.55f, carW, carH * 0.52f);
        const Rect cabinBody(leadCx - carW * 0.36f, leadY - carH * 0.95f, carW * 0.72f, carH * 0.45f);
        canvas.DrawRoundRect(cabinBody, 4.0f * s, Color(30, 41, 59, 235), radarCol, 1.2f * s);
        canvas.DrawRoundRect(lowerBody, 4.0f * s, Color(45, 58, 78, 245), radarCol, 1.4f * s);
        // Lead car tail lights
        canvas.DrawRect(Rect(lowerBody.x + 2.0f * s, lowerBody.y + 3.0f * s, carW * 0.22f, 3.0f * s), pal.dangerRed);
        canvas.DrawRect(Rect(lowerBody.Right() - 2.0f * s - carW * 0.22f, lowerBody.y + 3.0f * s, carW * 0.22f, 3.0f * s), pal.dangerRed);

        // Distance & speed floating badge above lead vehicle
        char distBuf[32]{};
        const keel::StringRef distRef = keel::format_buf(distBuf, "%.0fm · %.0f %s",
                      distM, DisplaySpeed(opts.adasLeadSpeedKmh, opts.speedUnit), SpeedUnitLabel(opts.speedUnit));
        const float badgeW = keel::min(bodyRect.w - 8.0f * s, 102.0f * s);
        const Rect badgeR(leadCx - badgeW * 0.5f, keel::max(bodyRect.y + 4.0f * s, cabinBody.y - 18.0f * s), badgeW, 15.0f * s);
        canvas.DrawRoundRect(badgeR, 4.0f * s, Color(9, 14, 24, 220), radarCol, 1.0f * s);
        canvas.DrawTextBox(distRef, badgeR, 9.0f * s, radarCol, 0.5f, 0.5f);
    }

    // Foreground Ego Vehicle Silhouette (with active brake lights & turn signals!)
    const float egoW = keel::clamp(bodyRect.w * 0.34f, 44.0f * s, 92.0f * s);
    const float egoH = egoW * 0.56f;
    const float egoY = botY - 6.0f * s;
    const Rect egoLower(cx - egoW * 0.5f, egoY - egoH * 0.55f, egoW, egoH * 0.52f);
    const Rect egoCabin(cx - egoW * 0.35f, egoY - egoH * 0.96f, egoW * 0.70f, egoH * 0.46f);

    // Shadow under ego car
    canvas.DrawRoundRect(Rect(cx - egoW * 0.54f, egoY - 3.0f * s, egoW * 1.08f, 6.0f * s), 3.0f * s, Color(0, 0, 0, 175));
    canvas.DrawRoundRect(egoCabin, 5.0f * s, Color(24, 34, 52, 245), pal.primaryAccent.WithAlpha(180), 1.2f * s);
    canvas.DrawRoundRect(egoLower, 5.0f * s, Color(34, 47, 68, 255), pal.primaryAccent, 1.5f * s);

    // Brake light strip on ego car
    const bool braking = (opts.brakePct > 4.0f || opts.lonG < -0.12f);
    const Color tailCol = braking ? Color(255, 42, 75, 255) : Color(190, 24, 52, 200);
    if (braking) {
        // Red brake glow halo
        canvas.DrawRoundRect(Rect(egoLower.x - 3.0f * s, egoLower.y + 1.0f * s, egoW + 6.0f * s, 9.0f * s), 4.0f * s, Color(244, 63, 94, 85));
    }
    canvas.DrawRect(Rect(egoLower.x + 3.0f * s, egoLower.y + 3.0f * s, egoW - 6.0f * s, 3.5f * s), tailCol);

    // Turn signal lamps on ego car corners
    if ((opts.telltales.leftTurn || opts.telltales.hazard) && opts.telltales.blinkPhaseOn) {
        canvas.DrawRect(Rect(egoLower.x + 2.0f * s, egoLower.y + 2.0f * s, 9.0f * s, 5.0f * s), pal.warningAmber);
    }
    if ((opts.telltales.rightTurn || opts.telltales.hazard) && opts.telltales.blinkPhaseOn) {
        canvas.DrawRect(Rect(egoLower.Right() - 11.0f * s, egoLower.y + 2.0f * s, 9.0f * s, 5.0f * s), pal.warningAmber);
    }

    // Top HUD Readout inside ADAS MFD: ACC Set Speed + Steering Angle
    char accBuf[48]{};
    const keel::StringRef accRef = keel::format_buf(accBuf, "ACC %.0f %s  |  STEER %+.0f deg",
                  DisplaySpeed(opts.cruiseTargetKmh, opts.speedUnit),
                  SpeedUnitLabel(opts.speedUnit),
                  opts.steeringAngleDeg);
    canvas.DrawTextBox(
        accRef,
        Rect(bodyRect.x + 6.0f * s, bodyRect.y + 4.0f * s, bodyRect.w - 12.0f * s, 16.0f * s),
        9.5f * s,
        pal.textSecondary,
        0.5f,
        0.5f
    );

    canvas.PopClip();
}

void RenderTpmsChassisPage(
    ICanvas& canvas,
    const Rect& bodyRect,
    const CarClusterViewOptions& opts,
    const ClusterPalette& pal,
    float s
) noexcept {
    canvas.PushClip(bodyRect);

    const Vec2 c = bodyRect.Center();
    const float chW = keel::min(bodyRect.w * 0.26f, 56.0f * s);
    const float chH = keel::min(bodyRect.h * 0.62f, 118.0f * s);
    const Rect chassisRect(c.x - chW * 0.5f, c.y - chH * 0.5f - 6.0f * s, chW, chH);

    // Axles & Drivetrain spine
    const float frontAxleY = chassisRect.y + chH * 0.22f;
    const float rearAxleY  = chassisRect.y + chH * 0.78f;
    canvas.DrawLine({chassisRect.x - 10.0f * s, frontAxleY}, {chassisRect.Right() + 10.0f * s, frontAxleY}, pal.dialRim, 2.0f * s);
    canvas.DrawLine({chassisRect.x - 10.0f * s, rearAxleY},  {chassisRect.Right() + 10.0f * s, rearAxleY},  pal.dialRim, 2.0f * s);
    canvas.DrawLine({c.x, frontAxleY}, {c.x, rearAxleY}, pal.primaryAccent.WithAlpha(180), 2.5f * s);

    // Chassis body outline
    canvas.DrawRoundRect(chassisRect, 10.0f * s, Color(20, 29, 44, 220), pal.cardBorder, 1.4f * s);

    // 4 Tires: FL (0), FR (1), RL (2), RR (3)
    const Vec2 wheelCenters[4] = {
        {chassisRect.x - 8.0f * s, frontAxleY},
        {chassisRect.Right() + 8.0f * s, frontAxleY},
        {chassisRect.x - 8.0f * s, rearAxleY},
        {chassisRect.Right() + 8.0f * s, rearAxleY}
    };
    const char* wheelNames[4] = {"FL", "FR", "RL", "RR"};

    for (keel::i32 i = 0; i < 4; ++i) {
        const float bar = opts.tirePressureBar[static_cast<keel::usize>(i)];
        const float tmpC = opts.tireTempC[static_cast<keel::usize>(i)];
        const bool warn = (bar < 2.0f || bar > 3.1f || tmpC > 85.0f);
        const Color wCol = warn ? pal.warningAmber : pal.ecoGreen;

        const Rect tireR(wheelCenters[i].x - 5.0f * s, wheelCenters[i].y - 11.0f * s, 10.0f * s, 22.0f * s);
        canvas.DrawRoundRect(tireR, 3.0f * s, wCol.WithAlpha(65), wCol, 1.5f * s);

        // Callout card on Left or Right side
        const bool isLeft = (i % 2 == 0);
        const float cardW = keel::max(54.0f * s, (bodyRect.w - chW) * 0.40f);
        const float cardH = 34.0f * s;
        const float cardX = isLeft ? (bodyRect.x + 6.0f * s) : (bodyRect.Right() - 6.0f * s - cardW);
        const float cardY = wheelCenters[i].y - cardH * 0.5f;
        const Rect cardR(cardX, cardY, cardW, cardH);

        canvas.DrawRoundRect(cardR, 4.0f * s, pal.cardBg, wCol.WithAlpha(150), 1.0f * s);

        char line1[24]{};
        char line2[24]{};
        const keel::StringRef line1Ref = (opts.speedUnit == SpeedUnit::Mph)
            ? keel::format_buf(line1, "%s %.0f psi", wheelNames[i], DisplayPressure(bar, opts.speedUnit))
            : keel::format_buf(line1, "%s %.2f bar", wheelNames[i], bar);
        const keel::StringRef line2Ref = keel::format_buf(line2, "%.0f %s", DisplayTemp(tmpC, opts.speedUnit), TempUnitLabel(opts.speedUnit));

        canvas.DrawTextBox(line1Ref, Rect(cardR.x + 3.0f * s, cardR.y + 2.0f * s, cardR.w - 6.0f * s, 15.0f * s), 9.5f * s, pal.textBright, 0.5f, 0.5f);
        canvas.DrawTextBox(line2Ref, Rect(cardR.x + 3.0f * s, cardR.y + 17.0f * s, cardR.w - 6.0f * s, 14.0f * s), 8.8f * s, wCol, 0.5f, 0.5f);
    }

    // Bottom AWD Torque Split & Oil/Batt Temp Summary
    char splitBuf[48]{};
    const float frontPct = keel::clamp(opts.frontTorquePct, 0.0f, 100.0f);
    const keel::StringRef splitRef = keel::format_buf(splitBuf, "AWD SPLIT: F %.0f%% / R %.0f%%", frontPct, 100.0f - frontPct);
    canvas.DrawTextBox(
        splitRef,
        Rect(bodyRect.x + 6.0f * s, bodyRect.Bottom() - 20.0f * s, bodyRect.w - 12.0f * s, 16.0f * s),
        9.2f * s,
        pal.primaryAccent,
        0.5f,
        0.5f
    );

    canvas.PopClip();
}

void RenderGForceDynamicsPage(
    ICanvas& canvas,
    const Rect& bodyRect,
    const CarClusterViewOptions& opts,
    const ClusterPalette& pal,
    float s
) noexcept {
    canvas.PushClip(bodyRect);

    const float gRadius = keel::min(bodyRect.w * 0.36f, bodyRect.h * 0.34f);
    const Vec2 gCenter{bodyRect.Center().x, bodyRect.y + gRadius + 14.0f * s};

    // Concentric G rings: 0.5G, 1.0G, 1.5G
    DrawCircleRing(canvas, gCenter, gRadius, pal.dialInnerBg, pal.dialRim, 1.4f * s);
    DrawCircleRing(canvas, gCenter, gRadius * 0.66f, Color::Transparent(), pal.tickMinor, 0.9f * s);
    DrawCircleRing(canvas, gCenter, gRadius * 0.33f, Color::Transparent(), pal.tickMinor, 0.8f * s);
    canvas.DrawLine({gCenter.x - gRadius, gCenter.y}, {gCenter.x + gRadius, gCenter.y}, pal.tickMinor, 0.9f * s);
    canvas.DrawLine({gCenter.x, gCenter.y - gRadius}, {gCenter.x, gCenter.y + gRadius}, pal.tickMinor, 0.9f * s);

    // Live G-Vector puck (max scale = 1.5 G)
    const float normX = keel::clamp(opts.latG / 1.5f, -1.0f, 1.0f);
    const float normY = keel::clamp(-opts.lonG / 1.5f, -1.0f, 1.0f);
    const Vec2 puck{gCenter.x + normX * gRadius, gCenter.y + normY * gRadius};
    canvas.DrawLine(gCenter, puck, pal.primaryAccent, 2.0f * s);
    DrawCircleRing(canvas, puck, 5.0f * s, pal.dangerRed, pal.textBright, 1.4f * s);

    const float totalG = std::hypot(opts.latG, opts.lonG);
    char gBuf[48]{};
    const keel::StringRef gRef = keel::format_buf(gBuf, "%.2f G  (PEAK %.2f G)", totalG, keel::max(totalG, opts.peakG));
    canvas.DrawTextBox(
        gRef,
        Rect(bodyRect.x + 6.0f * s, gCenter.y + gRadius + 3.0f * s, bodyRect.w - 12.0f * s, 15.0f * s),
        9.5f * s,
        pal.textBright,
        0.5f,
        0.5f
    );

    // Pedal Bars: Throttle (Green/Cyan) & Brake (Crimson)
    const float barsY = gCenter.y + gRadius + 22.0f * s;
    const float barW = bodyRect.w - 24.0f * s;
    const float barX = bodyRect.x + 12.0f * s;
    if (barsY + 34.0f * s <= bodyRect.Bottom() && barW > 40.0f) {
        const float thrFrac = keel::clamp(opts.throttlePct * 0.01f, 0.0f, 1.0f);
        const float brkFrac = keel::clamp(opts.brakePct * 0.01f, 0.0f, 1.0f);

        const Rect thrTrack(barX, barsY, barW, 8.0f * s);
        canvas.DrawRoundRect(thrTrack, 3.0f * s, Color(24, 34, 48, 200), pal.cardBorder, 0.8f * s);
        if (thrFrac > 0.01f) {
            canvas.DrawRoundRect(Rect(barX, barsY, barW * thrFrac, 8.0f * s), 3.0f * s, pal.ecoGreen);
        }

        const Rect brkTrack(barX, barsY + 12.0f * s, barW, 8.0f * s);
        canvas.DrawRoundRect(brkTrack, 3.0f * s, Color(24, 34, 48, 200), pal.cardBorder, 0.8f * s);
        if (brkFrac > 0.01f) {
            canvas.DrawRoundRect(Rect(barX, barsY + 12.0f * s, barW * brkFrac, 8.0f * s), 3.0f * s, pal.dangerRed);
        }

        char pedBuf[48]{};
        const keel::StringRef pedRef = keel::format_buf(pedBuf, "THR %.0f%%   |   BRK %.0f%%", opts.throttlePct, opts.brakePct);
        canvas.DrawTextBox(
            pedRef,
            Rect(barX, barsY + 21.0f * s, barW, 14.0f * s),
            8.8f * s,
            pal.textSecondary,
            0.5f,
            0.5f
        );
    }

    canvas.PopClip();
}

void RenderCenterMfd(
    ICanvas& canvas,
    const Rect& mfdRect,
    const CarClusterViewOptions& opts,
    const ClusterPalette& pal,
    float roadPhase,
    float s
) noexcept {
    if (mfdRect.w < 90.0f || mfdRect.h < 80.0f) return;

    canvas.DrawRoundRect(mfdRect, 10.0f * s, pal.cardBg, pal.cardBorder, 1.2f * s);

    const float headerH = 22.0f * s;
    const float footerH = 18.0f * s;
    const Rect headerRect(mfdRect.x, mfdRect.y, mfdRect.w, headerH);
    const Rect footerRect(mfdRect.x, mfdRect.Bottom() - footerH, mfdRect.w, footerH);
    const Rect bodyRect(mfdRect.x + 2.0f * s, mfdRect.y + headerH, mfdRect.w - 4.0f * s, keel::max(10.0f, mfdRect.h - headerH - footerH));

    const char* pageTitle = "ADAS 3D LANE & RADAR";
    switch (opts.centerMfd) {
        case ClusterCenterMfd::AdasLaneRadar:  pageTitle = "ADAS 3D LANE & RADAR"; break;
        case ClusterCenterMfd::TpmsChassis:    pageTitle = "TPMS & CHASSIS TELEMETRY"; break;
        case ClusterCenterMfd::GForceDynamics: pageTitle = "G-FORCE & DYNAMICS"; break;
    }
    canvas.DrawTextBox(pageTitle, headerRect, 9.5f * s, pal.primaryAccent, 0.5f, 0.5f);
    canvas.DrawLine({mfdRect.x + 8.0f * s, headerRect.Bottom()}, {mfdRect.Right() - 8.0f * s, headerRect.Bottom()}, pal.cardBorder, 1.0f * s);

    switch (opts.centerMfd) {
        case ClusterCenterMfd::AdasLaneRadar:
            RenderAdasLaneRadarPage(canvas, bodyRect, opts, pal, roadPhase, s);
            break;
        case ClusterCenterMfd::TpmsChassis:
            RenderTpmsChassisPage(canvas, bodyRect, opts, pal, s);
            break;
        case ClusterCenterMfd::GForceDynamics:
            RenderGForceDynamicsPage(canvas, bodyRect, opts, pal, s);
            break;
    }

    // Bottom Page Dots ([ADAS] [TPMS] [G-METER])
    const float dotW = 18.0f * s;
    const float dotH = 5.0f * s;
    const float dotGap = 6.0f * s;
    const float totalDotsW = dotW * 3.0f + dotGap * 2.0f;
    const float startX = footerRect.Center().x - totalDotsW * 0.5f;
    const float dotY = footerRect.Center().y - dotH * 0.5f;

    for (keel::i32 i = 0; i < 3; ++i) {
        const bool isCurrent = (static_cast<keel::i32>(opts.centerMfd) == i);
        const Rect dr(startX + static_cast<float>(i) * (dotW + dotGap), dotY, dotW, dotH);
        canvas.DrawRoundRect(dr, dotH * 0.5f, isCurrent ? pal.primaryAccent : Color(51, 65, 85, 160));
    }
}

// ============================================================================
// 6. TrackCentral & ModernWide Side Wing Panels
// ============================================================================
void RenderTrackLeftWing(
    ICanvas& canvas,
    const Rect& wingRect,
    const CarClusterViewOptions& opts,
    const ClusterPalette& pal,
    float s
) noexcept {
    canvas.DrawRoundRect(wingRect, 10.0f * s, pal.cardBg, pal.cardBorder, 1.2f * s);
    canvas.PushClip(wingRect.Inset(6.0f * s));

    const float x = wingRect.x + 12.0f * s;
    const float w = keel::max(20.0f, wingRect.w - 24.0f * s);
    float y = wingRect.y + 10.0f * s;

    // 1. Digital Speed & ACC Card
    canvas.DrawTextBox("GROUND SPEED", Rect(x, y, w, 14.0f * s), 9.0f * s, pal.textMuted, 0.0f, 0.5f);
    y += 14.0f * s;

    char spdStr[32]{};
    canvas.DrawTextBox(
        keel::format_buf(spdStr, "%.0f %s", DisplaySpeed(opts.speedKmh, opts.speedUnit), SpeedUnitLabel(opts.speedUnit)),
        Rect(x, y, w, 28.0f * s),
        22.0f * s,
        pal.textBright,
        0.0f,
        0.5f
    );
    y += 32.0f * s;

    // 2. Boost / Power Horizontal Bar
    const bool isIce = (opts.powertrain == PowertrainMode::IceTurbo);
    char boostLbl[48]{};
    float boostFrac = 0.0f;
    keel::StringRef boostRef{};
    if (isIce) {
        boostRef = keel::format_buf(boostLbl, "TURBO BOOST  %+.2f bar", opts.boostBar);
        boostFrac = keel::clamp((opts.boostBar + 0.5f) / 2.5f, 0.0f, 1.0f);
    } else {
        boostRef = keel::format_buf(boostLbl, "MOTOR POWER  %+.0f kW", opts.evPowerKw);
        boostFrac = keel::clamp((opts.evPowerKw + 150.0f) / 600.0f, 0.0f, 1.0f);
    }
    canvas.DrawTextBox(boostRef, Rect(x, y, w, 14.0f * s), 9.2f * s, pal.textSecondary, 0.0f, 0.5f);
    y += 15.0f * s;
    canvas.DrawRoundRect(Rect(x, y, w, 8.0f * s), 3.0f * s, Color(24, 34, 48, 220), pal.cardBorder, 0.8f * s);
    canvas.DrawRoundRect(Rect(x, y, w * boostFrac, 8.0f * s), 3.0f * s, pal.primaryAccent);
    y += 16.0f * s;

    // 3. Oil & Coolant / Battery Temperature Bars
    char t1Lbl[48]{};
    const keel::StringRef t1Ref = keel::format_buf(
        t1Lbl,
        "%s  %.0f %s",
        isIce ? "COOLANT TEMP" : "BATTERY TEMP",
        DisplayTemp(opts.coolantOrBattTempC, opts.speedUnit),
        TempUnitLabel(opts.speedUnit)
    );
    canvas.DrawTextBox(t1Ref, Rect(x, y, w, 14.0f * s), 9.2f * s, pal.textSecondary, 0.0f, 0.5f);
    y += 15.0f * s;
    const float cFrac = keel::clamp((opts.coolantOrBattTempC - 30.0f) / 95.0f, 0.0f, 1.0f);
    canvas.DrawRoundRect(Rect(x, y, w, 7.0f * s), 3.0f * s, Color(24, 34, 48, 220), pal.cardBorder, 0.8f * s);
    canvas.DrawRoundRect(Rect(x, y, w * cFrac, 7.0f * s), 3.0f * s, pal.secondaryAccent);
    y += 15.0f * s;

    char t2Lbl[48]{};
    const keel::StringRef t2Ref = keel::format_buf(
        t2Lbl,
        "OIL TEMP  %.0f %s",
        DisplayTemp(opts.oilTempC, opts.speedUnit),
        TempUnitLabel(opts.speedUnit)
    );
    canvas.DrawTextBox(t2Ref, Rect(x, y, w, 14.0f * s), 9.2f * s, pal.textSecondary, 0.0f, 0.5f);
    y += 15.0f * s;
    const float oFrac = keel::clamp((opts.oilTempC - 40.0f) / 100.0f, 0.0f, 1.0f);
    canvas.DrawRoundRect(Rect(x, y, w, 7.0f * s), 3.0f * s, Color(24, 34, 48, 220), pal.cardBorder, 0.8f * s);
    canvas.DrawRoundRect(Rect(x, y, w * oFrac, 7.0f * s), 3.0f * s, pal.warningAmber);
    y += 16.0f * s;

    // 4. Fuel / SoC & Range Bar
    char fLbl[48]{};
    const keel::StringRef fRef = keel::format_buf(
        fLbl,
        "%s  %.0f%%  (%.0f %s)",
        isIce ? "FUEL LEVEL" : "BATTERY SOC",
        opts.fuelOrSocPct,
        DisplayDistance(opts.estimatedRangeKm, opts.speedUnit),
        DistUnitLabel(opts.speedUnit)
    );
    canvas.DrawTextBox(fRef, Rect(x, y, w, 14.0f * s), 9.2f * s, pal.textSecondary, 0.0f, 0.5f);
    y += 15.0f * s;
    const float fFrac = keel::clamp(opts.fuelOrSocPct * 0.01f, 0.0f, 1.0f);
    canvas.DrawRoundRect(Rect(x, y, w, 7.0f * s), 3.0f * s, Color(24, 34, 48, 220), pal.cardBorder, 0.8f * s);
    canvas.DrawRoundRect(Rect(x, y, w * fFrac, 7.0f * s), 3.0f * s, pal.ecoGreen);

    canvas.PopClip();
}

// ============================================================================
// 7. Bottom PRNDSM Gear Selector & Trip Computer Ribbon
// ============================================================================
void RenderBottomTripBar(
    ICanvas& canvas,
    const Rect& botRect,
    const CarClusterViewOptions& opts,
    const ClusterPalette& pal,
    float s
) noexcept {
    if (botRect.w < 160.0f || botRect.h < 18.0f) return;

    canvas.DrawRoundRect(botRect, 6.0f * s, Color(10, 15, 25, 225), pal.cardBorder, 1.0f * s);

    // Center PRNDSM Ribbon
    const GearSelector kModes[6] = {
        GearSelector::Park, GearSelector::Reverse, GearSelector::Neutral,
        GearSelector::Drive, GearSelector::Sport, GearSelector::Manual
    };
    const char* kModeChars[6] = {"P", "R", "N", "D", "S", "M"};

    const float cellW = 24.0f * s;
    const float cellH = keel::min(botRect.h - 4.0f * s, 18.0f * s);
    const float gap = 4.0f * s;
    const float totalW = cellW * 6.0f + gap * 5.0f;
    const float startX = botRect.Center().x - totalW * 0.5f;
    const float cy = botRect.Center().y - cellH * 0.5f;

    for (keel::i32 i = 0; i < 6; ++i) {
        const bool active = (opts.gearSelector == kModes[i]);
        const Rect cr(startX + static_cast<float>(i) * (cellW + gap), cy, cellW, cellH);
        if (active) {
            canvas.DrawRoundRect(cr, 4.0f * s, pal.primaryAccent, pal.textBright, 1.0f * s);
        }
        char lbl[8]{};
        const keel::StringRef lblRef = (active && (kModes[i] == GearSelector::Drive || kModes[i] == GearSelector::Sport || kModes[i] == GearSelector::Manual))
            ? keel::format_buf(lbl, "%s%d", kModeChars[i], keel::clamp(opts.currentGear, 1, 8))
            : keel::StringRef(kModeChars[i]);
        canvas.DrawTextBox(lblRef, cr, 10.0f * s, active ? Color(9, 13, 22, 255) : pal.textMuted, 0.5f, 0.5f);
    }

    // Left Trip & Ambient Temperature Readout
    char leftInfo[64]{};
    const keel::StringRef leftRef = keel::format_buf(
        leftInfo,
        "OUT %.1f°%s   ·   AVG %.1f %s",
        DisplayTemp(opts.outsideTempC, opts.speedUnit),
        TempUnitLabel(opts.speedUnit),
        opts.avgConsumption,
        (opts.powertrain == PowertrainMode::ElectricDualMotor) ? "kWh/100" : "L/100"
    );
    const float sideW = keel::max(0.0f, startX - botRect.x - 16.0f * s);
    if (sideW > 60.0f * s) {
        canvas.DrawTextBox(leftRef, Rect(botRect.x + 10.0f * s, botRect.y, sideW, botRect.h), 9.5f * s, pal.textSecondary, 0.0f, 0.5f);
    }

    // Right Trip & Total Odometer Readout
    char rightInfo[64]{};
    const keel::StringRef rightRef = keel::format_buf(
        rightInfo,
        "TRIP %.1f %s   ·   ODO %06.0f %s",
        DisplayDistance(opts.tripDistanceKm, opts.speedUnit),
        DistUnitLabel(opts.speedUnit),
        DisplayDistance(static_cast<float>(opts.odometerKm), opts.speedUnit),
        DistUnitLabel(opts.speedUnit)
    );
    if (sideW > 60.0f * s) {
        canvas.DrawTextBox(rightRef, Rect(startX + totalW + 6.0f * s, botRect.y, sideW, botRect.h), 9.5f * s, pal.textSecondary, 1.0f, 0.5f);
    }
}

// ============================================================================
// 8. Node Render Callback
// ============================================================================
void ResolveClusterStageRects(
    const Rect& ctrlRect,
    const CarClusterViewOptions& opts,
    float s,
    Rect& outLeftGauge,
    Rect& outCenterMfd,
    Rect& outRightGauge
) noexcept {
    float topY = ctrlRect.y + 6.0f * s;
    float botY = ctrlRect.Bottom() - 6.0f * s;
    if (opts.showShiftLights && (botY - topY) > 160.0f) topY += 12.0f * s;
    if (opts.showTelltaleBar && (botY - topY) > 140.0f) topY += 26.0f * s;
    if (opts.showBottomTripBar && (botY - topY) > 140.0f) botY -= 30.0f * s;

    const float stageX = ctrlRect.x + 10.0f * s;
    const float stageW = keel::max(40.0f, ctrlRect.w - 20.0f * s);
    const float stageH = keel::max(40.0f, botY - topY);
    const float gap = 10.0f * s;

    if (opts.layoutMode == ClusterLayoutMode::DualDial) {
        const float dialSize = keel::min(stageH, (stageW - gap * 2.0f) * 0.36f);
        const float mfdW = keel::max(90.0f, stageW - dialSize * 2.0f - gap * 2.0f);
        outLeftGauge  = Rect(stageX, topY, dialSize, stageH);
        outCenterMfd  = Rect(stageX + dialSize + gap, topY + 4.0f * s, mfdW, stageH - 8.0f * s);
        outRightGauge = Rect(stageX + stageW - dialSize, topY, dialSize, stageH);
    } else if (opts.layoutMode == ClusterLayoutMode::TrackCentral) {
        const float centerDialW = keel::min(stageH * 1.04f, stageW * 0.40f);
        const float wingW = keel::max(80.0f, (stageW - centerDialW - gap * 2.0f) * 0.5f);
        outLeftGauge  = Rect(stageX, topY + 4.0f * s, wingW, stageH - 8.0f * s);
        outCenterMfd  = Rect(stageX + wingW + gap, topY, centerDialW, stageH);
        outRightGauge = Rect(stageX + stageW - wingW, topY + 4.0f * s, wingW, stageH - 8.0f * s);
    } else {
        // ModernWide: slightly compact side dials (28% each) + wide panoramic MFD (44%)
        const float dialSize = keel::min(stageH * 0.92f, (stageW - gap * 2.0f) * 0.28f);
        const float mfdW = keel::max(120.0f, stageW - dialSize * 2.0f - gap * 2.0f);
        outLeftGauge  = Rect(stageX, topY, dialSize, stageH);
        outCenterMfd  = Rect(stageX + dialSize + gap, topY + 2.0f * s, mfdW, stageH - 4.0f * s);
        outRightGauge = Rect(stageX + stageW - dialSize, topY, dialSize, stageH);
    }
}

void RenderCarClusterViewNode(const Node* node, ICanvas& canvas, const UiTheme& /*theme*/) noexcept {
    const auto* payload = node->As<CarClusterViewPayload>();
    if (!payload) return;

    const CarClusterViewOptions& opts = payload->opts;
    const CarClusterViewResponse& resp = payload->resp;

    const Rect ctrlRect = node->computedRect;
    if (ctrlRect.w <= 16.0f || ctrlRect.h <= 16.0f) return;

    constexpr float s = 1.0f;
    const ClusterPalette pal = ResolveClusterPalette(opts.driveTheme);
    const float outerRadius = opts.border.radius.has_value()
        ? *opts.border.radius
        : 14.0f;
    const Color binnacleBg = opts.background.Resolve(node->hovered, node->pressed, node->disabled, pal.binnacleBg);
    const Color binnacleBorder = opts.border.color.Resolve(node->hovered, node->pressed, node->disabled, pal.binnacleBorder);
    const float borderW = (opts.border.width > 0.0f) ? opts.border.width : 1.5f;

    // Outer automotive binnacle frame
    canvas.DrawRoundRect(ctrlRect, outerRadius, binnacleBg, binnacleBorder, borderW);
    canvas.PushClip(ctrlRect.Inset(keel::max(1.0f, borderW)));

    float topY = ctrlRect.y + 6.0f * s;
    float botY = ctrlRect.Bottom() - 6.0f * s;

    // 1. Top Shift-Lights Strip
    if (opts.showShiftLights && (botY - topY) > 160.0f) {
        const float slW = keel::min(ctrlRect.w * 0.46f, 320.0f * s);
        const Rect slRect(ctrlRect.Center().x - slW * 0.5f, topY, slW, 10.0f * s);
        RenderShiftLightStrip(canvas, slRect, opts, pal, s);
        topY += 12.0f * s;
    }

    // 2. Top ISO Telltale Bar
    if (opts.showTelltaleBar && (botY - topY) > 140.0f) {
        const Rect ttRect(ctrlRect.x + 8.0f * s, topY, ctrlRect.w - 16.0f * s, 22.0f * s);
        RenderTelltaleBar(canvas, ttRect, opts, pal, s);
        topY += 26.0f * s;
    }

    // 3. Bottom PRNDSM & Trip Bar
    if (opts.showBottomTripBar && (botY - topY) > 140.0f) {
        const float barH = 24.0f * s;
        const Rect tripRect(ctrlRect.x + 10.0f * s, botY - barH, ctrlRect.w - 20.0f * s, barH);
        RenderBottomTripBar(canvas, tripRect, opts, pal, s);
        botY -= (barH + 6.0f * s);
    }

    // 4. Main Instrument Stage Layout
    Rect leftGaugeRect{};
    Rect centerMfdRect{};
    Rect rightGaugeRect{};
    ResolveClusterStageRects(ctrlRect, opts, s, leftGaugeRect, centerMfdRect, rightGaugeRect);

    if (opts.layoutMode == ClusterLayoutMode::DualDial) {
        RenderSpeedometerDial(canvas, leftGaugeRect, opts, pal, s);
        RenderCenterMfd(canvas, centerMfdRect, opts, pal, payload->roadPhase, s);
        RenderTachOrPowerDial(canvas, rightGaugeRect, opts, pal, false, s);
    } else if (opts.layoutMode == ClusterLayoutMode::TrackCentral) {
        RenderTrackLeftWing(canvas, leftGaugeRect, opts, pal, s);
        RenderTachOrPowerDial(canvas, centerMfdRect, opts, pal, true, s);
        RenderCenterMfd(canvas, rightGaugeRect, opts, pal, payload->roadPhase, s);
    } else {
        // ModernWide: Left Speedometer + Wide Center MFD + Right Power/Tach
        RenderSpeedometerDial(canvas, leftGaugeRect, opts, pal, s);
        RenderCenterMfd(canvas, centerMfdRect, opts, pal, payload->roadPhase, s);
        RenderTachOrPowerDial(canvas, rightGaugeRect, opts, pal, false, s);
    }

    // 5. Optional Interactive Mouse Drive Reticle Overlay
    if (opts.showMouseDriveOverlay && resp.cursorInside) {
        const Vec2 c = ctrlRect.Center();
        const Vec2 curPt{
            c.x + resp.cursorNorm.x * (ctrlRect.w * 0.45f),
            c.y + resp.cursorNorm.y * (ctrlRect.h * 0.42f)
        };
        const Color guideCol = (resp.cursorBrakePct > 1.0f) ? pal.dangerRed : pal.primaryAccent;
        canvas.DrawLine({c.x - 14.0f * s, c.y}, {c.x + 14.0f * s, c.y}, pal.textMuted.WithAlpha(90), 1.0f * s);
        canvas.DrawLine({c.x, c.y - 14.0f * s}, {c.x, c.y + 14.0f * s}, pal.textMuted.WithAlpha(90), 1.0f * s);
        canvas.DrawLine(c, curPt, guideCol.WithAlpha(150), 1.4f * s);
        DrawCircleRing(canvas, curPt, 5.5f * s, guideCol.WithAlpha(55), guideCol, 1.5f * s);
    }

    canvas.PopClip();
}

} // namespace

Vec2 CarClusterView::Measure(
    const CarClusterViewOptions& opts,
    UiSize /*size*/,
    ICanvas* /*canvas*/,
    const UiTheme& /*theme*/,
    float availableWidth
) {
    float defaultW = 760.0f;
    if (opts.width.IsFixed()) {
        defaultW = opts.width.Dp();
    } else if (opts.width.ResolveMode(SizeMode::Stretch) == SizeMode::Stretch && availableWidth > 0.0f) {
        defaultW = availableWidth;
    }
    const float defaultH = opts.height.IsFixed()
        ? opts.height.Dp()
        : 340.0f;
    return {defaultW, defaultH};
}

CarClusterViewResponse CarClusterView::Run(
    Context& ctx,
    const CarClusterViewOptions& opts,
    keel::SourceLocation loc
) {

    const Id id = ctx.MakeChildId(loc);
    const bool isDisabled = opts.disabled || ctx.IsDisabled();
    const UiSize effectiveSize = opts.size;
    constexpr float s = 1.0f;

    Response baseRes = ctx.Interact(id);
    CarClusterViewResponse resp{};
    static_cast<Response&>(resp) = baseRes;
    resp.layoutMode = opts.layoutMode;
    resp.activeCenterMfd = opts.centerMfdPtr ? *opts.centerMfdPtr : opts.centerMfd;

    auto& state = ctx.GetOrCreatePersistentState<CarClusterViewState>(id);
    // Advance animated road scroll phase proportionally to vehicle speed
    const float speedMps = keel::max(0.0f, opts.speedKmh) / 3.6f;
    state.roadScrollPhase = std::fmod(state.roadScrollPhase + static_cast<double>(speedMps) * 0.0018, 1.0);

    Rect prevBounds = baseRes.rect;
    if (prevBounds.w <= 1.0f || prevBounds.h <= 1.0f) {
        prevBounds = ctx.GetParentPrevFrameBounds();
    }
    const Rect ctrlRect = prevBounds;
    resp.clusterRect = ctrlRect;

    if (ctrlRect.w > 1.0f && ctrlRect.h > 1.0f) {
        ResolveClusterStageRects(
            ctrlRect,
            opts,
            s,
            resp.leftGaugeRect,
            resp.centerMfdRect,
            resp.rightGaugeRect
        );

        const Vec2 mouse = ctx.GetMousePos();
        const bool hovered = !isDisabled && ctx.IsRectHovered(id, ctrlRect);
        resp.hovered = hovered;
        resp.cursorInside = hovered;

        if (hovered) {
            const Vec2 center = ctrlRect.Center();
            const float nx = keel::clamp((mouse.x - center.x) / keel::max(1.0f, ctrlRect.w * 0.45f), -1.0f, 1.0f);
            const float ny = keel::clamp((mouse.y - center.y) / keel::max(1.0f, ctrlRect.h * 0.42f), -1.0f, 1.0f);
            resp.cursorNorm = {nx, ny};
            resp.cursorSteerDeg = nx * 45.0f;
            if (ny < -0.04f) {
                resp.cursorThrottlePct = keel::clamp((-ny - 0.04f) / 0.96f, 0.0f, 1.0f) * 100.0f;
                resp.cursorBrakePct = 0.0f;
            } else if (ny > 0.04f) {
                resp.cursorThrottlePct = 0.0f;
                resp.cursorBrakePct = keel::clamp((ny - 0.04f) / 0.96f, 0.0f, 1.0f) * 100.0f;
            }

            // Click inside MFD area cycles MFD page if bound
            const Rect mfdHit = (opts.layoutMode == ClusterLayoutMode::TrackCentral) ? resp.rightGaugeRect : resp.centerMfdRect;
            if (baseRes.clicked && mfdHit.Contains(mouse)) {
                if (opts.centerMfdPtr) {
                    const keel::i32 nextPage = (static_cast<keel::i32>(*opts.centerMfdPtr) + 1) % 3;
                    *opts.centerMfdPtr = static_cast<ClusterCenterMfd>(nextPage);
                    resp.activeCenterMfd = *opts.centerMfdPtr;
                    resp.changed = true;
                }
            }

            // Mouse wheel shifts manual gear 1..8
            const float wheelY = ctx.GetMouseWheel().y;
            if (std::abs(wheelY) > 0.01f && opts.manualGearPtr) {
                const keel::i32 step = (wheelY > 0.0f) ? 1 : -1;
                const keel::i32 newGear = keel::clamp(*opts.manualGearPtr + step, 1, 8);
                if (newGear != *opts.manualGearPtr) {
                    *opts.manualGearPtr = newGear;
                    resp.changed = true;
                    ctx.RequestRepaint(2);
                }
                ctx.ConsumeMouseWheel();
            }
        }
    }

    if (resp.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, baseRes.rect);
    }

    Node* node = ctx.CreateNode(NodeType::CarClusterView, id);
    auto* payload = ctx.AllocPayload<CarClusterViewPayload>();
    payload->opts = opts;
    payload->opts.tooltip = ctx.AllocText(opts.tooltip);
    if (opts.centerMfdPtr) {
        payload->opts.centerMfd = *opts.centerMfdPtr;
    }
    if (opts.manualGearPtr) {
        payload->opts.currentGear = *opts.manualGearPtr;
    }
    payload->resp = resp;
    payload->roadPhase = static_cast<float>(state.roadScrollPhase);

    node->text = "CarClusterView";
    node->SetPayload(payload);

    const keel::u16 telltaleMask = static_cast<keel::u16>(
        (payload->opts.telltales.leftTurn             ? (1u << 0)  : 0u) |
        (payload->opts.telltales.rightTurn            ? (1u << 1)  : 0u) |
        (payload->opts.telltales.hazard               ? (1u << 2)  : 0u) |
        (payload->opts.telltales.lowBeam              ? (1u << 3)  : 0u) |
        (payload->opts.telltales.highBeam             ? (1u << 4)  : 0u) |
        (payload->opts.telltales.fogLight             ? (1u << 5)  : 0u) |
        (payload->opts.telltales.seatbeltWarning      ? (1u << 6)  : 0u) |
        (payload->opts.telltales.parkBrake            ? (1u << 7)  : 0u) |
        (payload->opts.telltales.absWarning           ? (1u << 8)  : 0u) |
        (payload->opts.telltales.escActive            ? (1u << 9)  : 0u) |
        (payload->opts.telltales.tpmsWarning          ? (1u << 10) : 0u) |
        (payload->opts.telltales.batteryOrEngineAlert ? (1u << 11) : 0u) |
        (payload->opts.telltales.adasCruiseActive     ? (1u << 12) : 0u) |
        (payload->opts.telltales.laneKeepActive       ? (1u << 13) : 0u) |
        (payload->opts.telltales.blinkPhaseOn         ? (1u << 14) : 0u)
    );

    node->visualHash = detail::HashArgs(
        static_cast<keel::u8>(payload->opts.layoutMode),
        static_cast<keel::u8>(payload->opts.powertrain),
        static_cast<keel::u8>(payload->opts.driveTheme),
        static_cast<keel::u8>(payload->opts.centerMfd),
        static_cast<keel::u8>(payload->opts.speedUnit),
        static_cast<keel::u8>(payload->opts.gearSelector),
        payload->opts.speedKmh,
        payload->opts.maxSpeedKmh,
        payload->opts.cruiseTargetKmh,
        payload->opts.cruiseSet,
        payload->opts.rpm,
        payload->opts.redlineRpm,
        payload->opts.maxRpm,
        payload->opts.currentGear,
        payload->opts.evPowerKw,
        payload->opts.throttlePct,
        payload->opts.brakePct,
        payload->opts.steeringAngleDeg,
        payload->opts.boostBar,
        payload->opts.fuelOrSocPct,
        payload->opts.estimatedRangeKm,
        payload->opts.coolantOrBattTempC,
        payload->opts.oilTempC,
        payload->opts.outsideTempC,
        payload->opts.odometerKm,
        payload->opts.tripDistanceKm,
        payload->opts.avgConsumption,
        payload->opts.latG,
        payload->opts.lonG,
        payload->opts.peakG,
        payload->opts.frontTorquePct,
        payload->opts.adasLeadDistanceM,
        payload->opts.adasLeadSpeedKmh,
        payload->opts.adasLeadDetected,
        payload->opts.tirePressureBar,
        payload->opts.tireTempC,
        payload->opts.showShiftLights,
        payload->opts.showTelltaleBar,
        payload->opts.showBottomTripBar,
        payload->opts.showMouseDriveOverlay,
        telltaleMask,
        payload->roadPhase,
        resp.cursorInside,
        resp.cursorNorm.x,
        resp.cursorNorm.y,
        opts.background,
        opts.border,
        ctrlRect.w,
        ctrlRect.h,
        effectiveSize,
        isDisabled
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* c, const UiTheme& th) noexcept -> Vec2 {
        const auto* p = n->As<CarClusterViewPayload>();
        if (!p) return {0.0f, 0.0f};
        CarClusterViewOptions effOpts = p->opts;
        if (n->widthMode == SizeMode::Stretch && !effOpts.width.IsFixed()) {
            effOpts.width = SizeMode::Stretch;
        }
        return CarClusterView::Measure(effOpts, n->uiSize, c, th, availableWidth);
    };
    node->renderFn = &RenderCarClusterViewNode;

    node->uiSize = effectiveSize;
    node->disabled = isDisabled;
    node->hovered = resp.hovered;
    node->pressed = baseRes.pressed;

    ControlSpec ctrlSpec{};
    ctrlSpec.width = opts.width;
    ctrlSpec.height = opts.height;
    ctrlSpec.defaultWidthMode = SizeMode::Stretch;
    ctrlSpec.defaultHeightMode = SizeMode::Stretch;
    ctrlSpec.minHeight = opts.height.IsFixed() ? opts.height.Dp() : 260.0f;
    Control::SetupNode(ctx, node, ctrlSpec);

    return resp;
}

} // namespace guikit
