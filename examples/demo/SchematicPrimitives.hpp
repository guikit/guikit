#pragma once

#include <algorithm>
#include <cmath>
#include <string_view>
#include "guikit/panels/Scene.hpp"

namespace guikit::demo {

/// Visual palette and stroke metrics for BS 3939 / IEC electronic schematic rendering.
struct SchematicPalette {
    Color sheetBg{250, 248, 242, 255};      ///< Drafting vellum / blueprint background
    Color bodyFill{250, 248, 242, 255};     ///< Opaque fill inside component bodies to mask underlying wires
    Color ink{24, 28, 36, 255};             ///< Primary schematic line and symbol stroke
    Color inkBold{12, 16, 22, 255};         ///< Heavy symbol features (transistor base bar, cap plate, pins)
    Color inkMuted{71, 85, 105, 230};       ///< Dashed PCB boundaries, table dividers, secondary notes
    Color accent{180, 83, 9, 255};          ///< Voltage test points & reference highlights
    float wireWidth{1.15f};
    float symbolWidth{1.35f};
    float heavyWidth{2.6f};

    /// Classic British drafting vellum (Light / Vintage Paper) palette.
    static SchematicPalette VintagePaper() noexcept {
        return SchematicPalette{
            .sheetBg    = Color(248, 246, 238, 255),
            .bodyFill   = Color(248, 246, 238, 255),
            .ink        = Color(22, 27, 34, 255),
            .inkBold    = Color(10, 14, 20, 255),
            .inkMuted   = Color(75, 85, 99, 235),
            .accent     = Color(153, 27, 27, 255)
        };
    }

    /// Modern Dark CAD / Blueprint palette adapting to dark UI themes.
    static SchematicPalette DarkCad() noexcept {
        return SchematicPalette{
            .sheetBg    = Color(13, 20, 33, 255),
            .bodyFill   = Color(15, 23, 42, 255),
            .ink        = Color(226, 232, 240, 255),
            .inkBold    = Color(248, 250, 252, 255),
            .inkMuted   = Color(148, 163, 184, 210),
            .accent     = Color(56, 189, 248, 255)
        };
    }
};

/// Single-header vector schematic & electronic symbol library built on top of `guikit::SceneScope`.
/// Implements British Standard BS 3939 / IEC symbols as used in the QUAD 303 (M12160) service manual.
class SchematicPainter {
public:
    SchematicPainter(SceneScope& scene, const SchematicPalette& palette) noexcept
        : m_scene(scene)
        , m_pal(palette)
    {}

    [[nodiscard]] const SchematicPalette& Palette() const noexcept { return m_pal; }

    // =========================================================================
    // 1. WIRES, BUSES, JUNCTIONS & BOARD TERMINALS
    // =========================================================================

    void Wire(Vec2 a, Vec2 b) {
        m_scene.DrawLine(a, b, m_pal.ink, m_pal.wireWidth);
    }

    void WireH(float x1, float x2, float y) {
        m_scene.DrawLine({x1, y}, {x2, y}, m_pal.ink, m_pal.wireWidth);
    }

    void WireV(float x, float y1, float y2) {
        m_scene.DrawLine({x, y1}, {x, y2}, m_pal.ink, m_pal.wireWidth);
    }

    void WireOrtho(Vec2 a, Vec2 b, bool horizFirst = true) {
        const Vec2 corner = horizFirst ? Vec2{b.x, a.y} : Vec2{a.x, b.y};
        Wire(a, corner);
        Wire(corner, b);
    }

    /// Draws a semicircular wire bridge crossover along a vertical wire crossing a horizontal wire at `yCross`.
    void WireHopV(float x, float yCross, float r = 4.5f) {
        m_scene.DrawRect(Rect(x - 2.0f, yCross - r, 4.0f, r * 2.0f), m_pal.bodyFill);
        WireH(x - r, x + r, yCross);
        m_scene.DrawArc({x, yCross}, r, -90.0f, 180.0f, m_pal.ink, m_pal.wireWidth, 10);
    }

    void DashedLine(Vec2 a, Vec2 b, float dashLen = 8.0f, float gapLen = 5.0f, float width = 1.25f) {
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float len = std::hypot(dx, dy);
        if (len < 0.5f) return;
        const float ux = dx / len;
        const float uy = dy / len;
        const float step = std::max(2.0f, dashLen + gapLen);

        for (float t = 0.0f; t < len; t += step) {
            const float tEnd = std::min(len, t + dashLen);
            m_scene.DrawLine(
                {a.x + ux * t, a.y + uy * t},
                {a.x + ux * tEnd, a.y + uy * tEnd},
                m_pal.inkMuted,
                width
            );
        }
    }

    void DashedRect(Rect r, float dashLen = 9.0f, float gapLen = 5.0f, float width = 1.35f) {
        DashedLine({r.x, r.y}, {r.Right(), r.y}, dashLen, gapLen, width);
        DashedLine({r.Right(), r.y}, {r.Right(), r.Bottom()}, dashLen, gapLen, width);
        DashedLine({r.Right(), r.Bottom()}, {r.x, r.Bottom()}, dashLen, gapLen, width);
        DashedLine({r.x, r.Bottom()}, {r.x, r.y}, dashLen, gapLen, width);
    }

    /// Draws a solid solder junction dot at a wire intersection.
    void Junction(Vec2 pt, float radius = 2.4f) {
        m_scene.DrawCircle(pt, radius, m_pal.inkBold);
    }

    /// Draws a numbered PCB edge terminal pin inside a circle (`1`..`15`).
    void BoardPin(Vec2 pt, std::string_view number) {
        auto item = m_scene.Item({
            .id = number,
            .title = "PCB Edge Terminal",
            .category = "Board Pin"
        });
        constexpr float r = 6.2f;
        m_scene.DrawCircle(pt, r, m_pal.bodyFill, m_pal.inkBold, m_pal.symbolWidth);
        m_scene.DrawTextBox(
            number,
            Rect(pt.x - r, pt.y - r, r * 2.0f, r * 2.0f),
            6.0f,
            m_pal.inkBold,
            0.5f,
            0.5f,
            FontStyle::Bold
        );
    }

    /// Draws a small hollow solder pad circle (used on PCB layout and switches).
    void SolderPad(Vec2 pt, float r = 3.0f) {
        m_scene.DrawCircle(pt, r, m_pal.bodyFill, m_pal.inkBold, 1.2f);
    }

    /// Draws a British Standard raked chassis earth symbol.
    void ChassisEarth(Vec2 pt) {
        WireV(pt.x, pt.y, pt.y + 7.0f);
        const float by = pt.y + 7.0f;
        m_scene.DrawLine({pt.x - 7.5f, by}, {pt.x + 7.5f, by}, m_pal.inkBold, m_pal.symbolWidth);
        m_scene.DrawLine({pt.x - 5.5f, by}, {pt.x - 9.0f, by + 5.5f}, m_pal.ink, 1.15f);
        m_scene.DrawLine({pt.x,        by}, {pt.x - 3.5f, by + 5.5f}, m_pal.ink, 1.15f);
        m_scene.DrawLine({pt.x + 5.5f, by}, {pt.x + 2.0f, by + 5.5f}, m_pal.ink, 1.15f);
    }

    /// Draws a hollow triangular off-sheet signal arrow (e.g. "TO RIGHTHAND AMPLIFIER").
    void SignalArrowDown(Vec2 pt, std::string_view line1 = "", std::string_view line2 = "") {
        m_scene.DrawTriangle(
            {pt.x, pt.y},
            {pt.x - 3.6f, pt.y - 7.2f},
            {pt.x + 3.6f, pt.y - 7.2f},
            m_pal.bodyFill,
            m_pal.inkBold,
            1.2f
        );
        if (!line1.empty()) {
            m_scene.DrawTextBox(
                line1,
                Rect(pt.x - 48.0f, pt.y + 3.0f, 96.0f, 8.0f),
                5.6f,
                m_pal.inkBold,
                0.5f,
                0.5f,
                FontStyle::Bold
            );
            m_scene.DrawLine({pt.x - 30.0f, pt.y + 12.5f}, {pt.x + 30.0f, pt.y + 12.5f}, m_pal.ink, 0.85f);
        }
        if (!line2.empty()) {
            m_scene.DrawTextBox(
                line2,
                Rect(pt.x - 48.0f, pt.y + 14.0f, 96.0f, 8.0f),
                5.6f,
                m_pal.inkBold,
                0.5f,
                0.5f,
                FontStyle::Bold
            );
            m_scene.DrawLine({pt.x - 22.0f, pt.y + 23.5f}, {pt.x + 22.0f, pt.y + 23.5f}, m_pal.ink, 0.85f);
        }
    }

    /// Helper to draw an aligned two-line or single-line component label on the left (`side < 0`) or right (`side > 0`).
    void SideLabel(Vec2 anchor, int side, std::string_view line1, std::string_view line2 = "") {
        const float boxW = 48.0f;
        const float alignX = (side >= 0) ? 0.0f : 1.0f;
        const float rx = (side >= 0) ? anchor.x : (anchor.x - boxW);
        if (line2.empty()) {
            m_scene.DrawTextBox(line1, Rect(rx, anchor.y - 5.0f, boxW, 10.0f), 6.0f, m_pal.inkBold, alignX, 0.5f, FontStyle::Bold);
        } else {
            m_scene.DrawTextBox(line1, Rect(rx, anchor.y - 8.5f, boxW, 8.5f), 6.0f, m_pal.inkBold, alignX, 0.5f, FontStyle::Bold);
            m_scene.DrawTextBox(line2, Rect(rx, anchor.y + 0.5f, boxW, 8.5f), 5.6f, m_pal.ink,     alignX, 0.5f, FontStyle::Bold);
        }
    }

    // =========================================================================
    // 2. PASSIVE COMPONENTS (BS 3939: RESISTORS, POTS, FUSES, CAPS, INDUCTORS)
    // =========================================================================

    /// Draws a BS 3939 rectangular resistor with the ohmic value printed inside the body
    /// (rotated -90 deg when vertical!) and the reference designator (`R104`, `R122`) outside.
    /// `labelSide`: +1 = right/bottom, -1 = left/top.
    void Resistor(
        Vec2 center,
        bool vertical,
        std::string_view refDes,
        std::string_view value,
        int labelSide = 1,
        bool dashed = false
    ) {
        auto item = m_scene.Item({
            .id = refDes.empty() ? value : refDes,
            .title = value,
            .category = dashed ? "Load Impedance" : "Resistor"
        });
        const float w = vertical ? 11.5f : 32.0f;
        const float h = vertical ? 32.0f : 11.5f;
        const Rect box(center.x - w * 0.5f, center.y - h * 0.5f, w, h);

        if (dashed) {
            m_scene.DrawRect(box, m_pal.bodyFill);
            DashedRect(box, 3.5f, 2.2f, 1.15f);
        } else {
            m_scene.DrawRect(box, m_pal.bodyFill, 0.0f, m_pal.inkBold, m_pal.symbolWidth);
        }

        if (!value.empty()) {
            m_scene.DrawTextBox(
                value,
                box,
                5.5f,
                m_pal.inkBold,
                0.5f,
                0.5f,
                FontStyle::Bold,
                true,
                vertical ? -90.0f : 0.0f
            );
        }

        if (!refDes.empty()) {
            if (vertical) {
                const float ax = (labelSide >= 0) ? (box.Right() + 3.0f) : (box.x - 3.0f);
                SideLabel({ax, center.y}, labelSide, refDes);
            } else {
                const float ly = (labelSide >= 0) ? (box.Bottom() + 2.0f) : (box.y - 10.5f);
                m_scene.DrawTextBox(
                    refDes,
                    Rect(center.x - 24.0f, ly, 48.0f, 9.0f),
                    6.0f,
                    m_pal.inkBold,
                    0.5f,
                    0.5f,
                    FontStyle::Bold
                );
            }
        }
    }

    /// Draws a BS 3939 preset trimmer potentiometer (`RV100`, `RV101`, `RV200`) with diagonal shaft and T-bar stop.
    /// `labelSide`: -1 = left, +1 = right, 2 = below body.
    void Potentiometer(
        Vec2 center,
        bool vertical,
        std::string_view refDes,
        std::string_view value,
        int labelSide = -1
    ) {
        auto item = m_scene.Item({
            .id = refDes,
            .title = value,
            .category = "Preset Trimmer"
        });
        const float w = vertical ? 11.0f : 32.0f;
        const float h = vertical ? 32.0f : 11.0f;
        const Rect box(center.x - w * 0.5f, center.y - h * 0.5f, w, h);
        m_scene.DrawRect(box, m_pal.bodyFill, 0.0f, m_pal.inkBold, m_pal.symbolWidth);

        // Diagonal preset shaft across body with BS 3939 preset T-bar at top-right
        const Vec2 p1 = {center.x - 9.5f, center.y + 11.0f};
        const Vec2 p2 = {center.x + 9.5f, center.y - 11.0f};
        m_scene.DrawLine(p1, p2, m_pal.inkBold, 1.2f);
        m_scene.DrawLine({p2.x - 2.8f, p2.y - 2.4f}, {p2.x + 2.8f, p2.y + 2.4f}, m_pal.inkBold, 1.4f);

        if (!refDes.empty()) {
            if (labelSide == 2) {
                m_scene.DrawTextBox(refDes, Rect(center.x - 24.0f, box.Bottom() + 14.0f, 48.0f, 8.5f), 6.0f, m_pal.inkBold, 0.5f, 0.5f, FontStyle::Bold);
                if (!value.empty()) {
                    m_scene.DrawTextBox(value, Rect(center.x - 24.0f, box.Bottom() + 22.5f, 48.0f, 8.5f), 5.6f, m_pal.ink, 0.5f, 0.5f, FontStyle::Bold);
                }
            } else {
                const float ax = (labelSide >= 0) ? (center.x + 11.0f) : (center.x - 11.0f);
                SideLabel({ax, center.y}, labelSide, refDes, value);
            }
        }
    }

    /// Draws a BS 3939 cartridge fuse (`FS1`) with continuous conductor passing through the body.
    void Fuse(Vec2 center, bool vertical, std::string_view refDes, int labelSide = 1) {
        auto item = m_scene.Item({
            .id = refDes,
            .title = "Mains Fuse",
            .category = "Fuse"
        });
        const float w = vertical ? 10.5f : 28.0f;
        const float h = vertical ? 28.0f : 10.5f;
        const Rect box(center.x - w * 0.5f, center.y - h * 0.5f, w, h);
        m_scene.DrawRect(box, m_pal.bodyFill, 0.0f, m_pal.inkBold, m_pal.symbolWidth);
        if (vertical) {
            WireV(center.x, box.y, box.Bottom());
            if (!refDes.empty()) {
                const float ax = (labelSide >= 0) ? (box.Right() + 3.0f) : (box.x - 3.0f);
                SideLabel({ax, center.y}, labelSide, refDes);
            }
        } else {
            WireH(box.x, box.Right(), center.y);
        }
    }

    /// Draws a non-polarized or polarized electrolytic capacitor (BS 3939 hollow + solid plate).
    /// `posSide`: -1 = top/left plate is positive (hollow), +1 = bottom/right plate is positive.
    /// `labelSide`: -1 = left/top, +1 = right/bottom, 2 = below vertical plates.
    void Capacitor(
        Vec2 center,
        bool vertical,
        std::string_view refDes,
        std::string_view value,
        bool electrolytic = false,
        int posSide = -1,
        int labelSide = -1
    ) {
        auto item = m_scene.Item({
            .id = refDes,
            .title = value,
            .category = electrolytic ? "Electrolytic Cap" : "Capacitor"
        });
        if (vertical) {
            m_scene.DrawRect(Rect(center.x - 8.5f, center.y - 3.8f, 17.0f, 7.6f), m_pal.bodyFill);
            if (!electrolytic) {
                m_scene.DrawRect(Rect(center.x - 7.5f, center.y - 3.4f, 15.0f, 2.2f), m_pal.inkBold);
                m_scene.DrawRect(Rect(center.x - 7.5f, center.y + 1.2f, 15.0f, 2.2f), m_pal.inkBold);
            } else {
                const float topY = center.y - 4.2f;
                const float botY = center.y + 1.2f;
                if (posSide <= 0) {
                    m_scene.DrawRect(Rect(center.x - 7.5f, topY, 15.0f, 3.0f), m_pal.bodyFill, 0.0f, m_pal.inkBold, 1.2f);
                    m_scene.DrawRect(Rect(center.x - 7.5f, botY, 15.0f, 2.8f), m_pal.inkBold);
                } else {
                    m_scene.DrawRect(Rect(center.x - 7.5f, topY, 15.0f, 2.8f), m_pal.inkBold);
                    m_scene.DrawRect(Rect(center.x - 7.5f, botY, 15.0f, 3.0f), m_pal.bodyFill, 0.0f, m_pal.inkBold, 1.2f);
                }
            }
            if (labelSide == 2) {
                SideLabel({center.x - 4.0f, center.y + 34.0f}, -1, refDes, value);
            } else {
                const float ax = (labelSide >= 0) ? (center.x + 9.5f) : (center.x - 9.5f);
                SideLabel({ax, center.y}, labelSide, refDes, value);
            }
        } else {
            m_scene.DrawRect(Rect(center.x - 3.8f, center.y - 8.5f, 7.6f, 17.0f), m_pal.bodyFill);
            if (!electrolytic) {
                m_scene.DrawRect(Rect(center.x - 3.4f, center.y - 7.5f, 2.2f, 15.0f), m_pal.inkBold);
                m_scene.DrawRect(Rect(center.x + 1.2f, center.y - 7.5f, 2.2f, 15.0f), m_pal.inkBold);
            } else {
                const float leftX = center.x - 4.2f;
                const float rightX = center.x + 1.2f;
                if (posSide <= 0) {
                    m_scene.DrawRect(Rect(leftX, center.y - 7.5f, 3.0f, 15.0f), m_pal.bodyFill, 0.0f, m_pal.inkBold, 1.2f);
                    m_scene.DrawRect(Rect(rightX, center.y - 7.5f, 2.8f, 15.0f), m_pal.inkBold);
                } else {
                    m_scene.DrawRect(Rect(leftX, center.y - 7.5f, 2.8f, 15.0f), m_pal.inkBold);
                    m_scene.DrawRect(Rect(rightX, center.y - 7.5f, 3.0f, 15.0f), m_pal.bodyFill, 0.0f, m_pal.inkBold, 1.2f);
                }
            }
            if (!refDes.empty() && !value.empty() && labelSide < 0) {
                m_scene.DrawTextBox(refDes, Rect(center.x - 24.0f, center.y - 17.5f, 48.0f, 8.5f), 6.0f, m_pal.inkBold, 0.5f, 0.5f, FontStyle::Bold);
                m_scene.DrawTextBox(value,  Rect(center.x - 24.0f, center.y + 9.0f,  48.0f, 8.5f), 5.6f, m_pal.ink,     0.5f, 0.5f, FontStyle::Bold);
            } else {
                const float ly = (labelSide >= 0) ? (center.y + 9.0f) : (center.y - 18.0f);
                if (!refDes.empty()) m_scene.DrawTextBox(refDes, Rect(center.x - 24.0f, ly, 48.0f, 8.5f), 6.0f, m_pal.inkBold, 0.5f, 0.5f, FontStyle::Bold);
                if (!value.empty())  m_scene.DrawTextBox(value,  Rect(center.x - 24.0f, ly + 8.0f, 48.0f, 8.5f), 5.6f, m_pal.ink, 0.5f, 0.5f, FontStyle::Bold);
            }
        }
    }

    /// Draws a 4-loop horizontal inductor coil (`L100`).
    void InductorH(Vec2 center, std::string_view refDes, float totalWidth = 38.0f) {
        auto item = m_scene.Item({
            .id = refDes,
            .title = "Output Choke",
            .category = "Inductor"
        });
        const int loops = 4;
        const float r = totalWidth / (static_cast<float>(loops) * 2.0f);
        m_scene.DrawRect(Rect(center.x - totalWidth * 0.5f, center.y - r - 2.0f, totalWidth, r + 4.0f), m_pal.bodyFill);
        for (int i = 0; i < loops; ++i) {
            const float cx = (center.x - totalWidth * 0.5f) + r * (2.0f * static_cast<float>(i) + 1.0f);
            m_scene.DrawArc({cx, center.y}, r, 180.0f, 180.0f, m_pal.inkBold, m_pal.symbolWidth, 10);
        }
        if (!refDes.empty()) {
            m_scene.DrawTextBox(refDes, Rect(center.x - 24.0f, center.y - r - 11.0f, 48.0f, 9.0f), 6.2f, m_pal.inkBold, 0.5f, 0.5f, FontStyle::Bold);
        }
    }

    // =========================================================================
    // 3. SEMICONDUCTORS (BJT TRANSISTORS, DIODES, ZENER, RECTIFIER BRIDGE)
    // =========================================================================

    /// Draws a semiconductor diode or Zener diode along any orientation (`0` = Right, `90` = Down, `180` = Left, `270` = Up).
    void Diode(
        Vec2 center,
        float dirDeg,
        std::string_view refDes,
        std::string_view partNum = "",
        bool isZener = false,
        int labelSide = 1
    ) {
        std::optional<SceneItemScope> itemScope;
        if (!refDes.empty()) {
            itemScope.emplace(m_scene.Item({
                .id = refDes,
                .title = partNum,
                .category = isZener ? "Zener Reference" : "Diode"
            }));
        }
        constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
        const float rad = dirDeg * kDegToRad;
        const Vec2 dir{std::cos(rad), std::sin(rad)};
        const Vec2 nrm{-dir.y, dir.x};

        const float halfL = 5.2f;
        const float halfW = 5.0f;

        const Vec2 tip  = {center.x + dir.x * halfL, center.y + dir.y * halfL};
        const Vec2 base = {center.x - dir.x * halfL, center.y - dir.y * halfL};
        const Vec2 b1   = {base.x + nrm.x * halfW, base.y + nrm.y * halfW};
        const Vec2 b2   = {base.x - nrm.x * halfW, base.y - nrm.y * halfW};

        m_scene.DrawTriangle(tip, b1, b2, m_pal.inkBold);

        // Cathode bar
        const Vec2 c1 = {tip.x + nrm.x * (halfW + 0.8f), tip.y + nrm.y * (halfW + 0.8f)};
        const Vec2 c2 = {tip.x - nrm.x * (halfW + 0.8f), tip.y - nrm.y * (halfW + 0.8f)};
        m_scene.DrawLine(c1, c2, m_pal.inkBold, m_pal.symbolWidth * 1.25f);

        if (isZener) {
            m_scene.DrawLine(c1, {c1.x - dir.x * 3.5f, c1.y - dir.y * 3.5f}, m_pal.inkBold, m_pal.symbolWidth);
            m_scene.DrawLine(c2, {c2.x + dir.x * 3.5f, c2.y + dir.y * 3.5f}, m_pal.inkBold, m_pal.symbolWidth);
        }

        if (!refDes.empty()) {
            if (std::abs(dir.x) < 0.25f) {
                const float ax = (labelSide >= 0) ? (center.x + 8.0f) : (center.x - 8.0f);
                SideLabel({ax, center.y}, labelSide, refDes, partNum);
            } else if (std::abs(dir.y) < 0.25f) {
                if (labelSide == 0) {
                    // Split refDes above and partNum below the horizontal diode
                    m_scene.DrawTextBox(refDes,  Rect(center.x - 28.0f, center.y - 15.0f, 56.0f, 8.0f), 5.8f, m_pal.inkBold, 0.5f, 0.5f, FontStyle::Bold);
                    m_scene.DrawTextBox(partNum, Rect(center.x - 28.0f, center.y + 7.0f,  56.0f, 8.0f), 5.5f, m_pal.ink,     0.5f, 0.5f, FontStyle::Bold);
                } else {
                    const float ly = (labelSide >= 0) ? (center.y + 7.0f) : (center.y - 16.5f);
                    m_scene.DrawTextBox(refDes, Rect(center.x - 28.0f, ly, 56.0f, 8.0f), 5.8f, m_pal.inkBold, 0.5f, 0.5f, FontStyle::Bold);
                    if (!partNum.empty()) {
                        m_scene.DrawTextBox(partNum, Rect(center.x - 28.0f, ly + 7.5f, 56.0f, 8.0f), 5.5f, m_pal.ink, 0.5f, 0.5f, FontStyle::Bold);
                    }
                }
            } else {
                const float ax = (labelSide >= 0) ? (center.x + 11.0f) : (center.x - 11.0f);
                SideLabel({ax, center.y - 4.0f}, labelSide, refDes, partNum);
            }
        }
    }

    /// Draws a British Standard BJT transistor inside an envelope circle (`r = 15.0`).
    /// Connects cleanly to:
    /// - Base terminal at `{center.x + (baseLeft ? -15.0 : +15.0), center.y}`
    /// - Top collector/emitter terminal at `{center.x, center.y - 15.0}`
    /// - Bottom collector/emitter terminal at `{center.x, center.y + 15.0}`
    void TransistorBJT(
        Vec2 center,
        bool isPNP,
        std::string_view refDes,
        std::string_view partNum,
        bool baseLeft = true,
        bool emitterAtTop = false,
        int labelSide = -1
    ) {
        auto item = m_scene.Item({
            .id = refDes,
            .title = partNum,
            .category = isPNP ? "PNP Transistor" : "NPN Transistor"
        });
        constexpr float r = 15.0f;
        m_scene.DrawCircle(center, r, m_pal.bodyFill, m_pal.inkBold, m_pal.symbolWidth * 1.15f);

        const float sgn = baseLeft ? -1.0f : 1.0f;
        const float barX = center.x + sgn * 4.5f;

        // Base lead from circle boundary to internal base bar
        m_scene.DrawLine({center.x + sgn * r, center.y}, {barX, center.y}, m_pal.inkBold, m_pal.symbolWidth);

        // Heavy internal base bar
        m_scene.DrawLine({barX, center.y - 8.2f}, {barX, center.y + 8.2f}, m_pal.inkBold, m_pal.heavyWidth);

        // Diagonal internal leads to top (x=center.x, y=center.y-r) and bottom (x=center.x, y=center.y+r)
        const Vec2 baseTop{barX, center.y - 4.0f};
        const Vec2 baseBot{barX, center.y + 4.0f};
        const Vec2 termTop{center.x, center.y - 11.5f};
        const Vec2 termBot{center.x, center.y + 11.5f};

        m_scene.DrawLine(baseTop, termTop, m_pal.inkBold, m_pal.symbolWidth);
        m_scene.DrawLine(termTop, {center.x, center.y - r}, m_pal.inkBold, m_pal.symbolWidth);

        m_scene.DrawLine(baseBot, termBot, m_pal.inkBold, m_pal.symbolWidth);
        m_scene.DrawLine(termBot, {center.x, center.y + r}, m_pal.inkBold, m_pal.symbolWidth);

        // Emitter arrow on either the top or bottom diagonal leg
        const Vec2 legStart = emitterAtTop ? baseTop : baseBot;
        const Vec2 legEnd   = emitterAtTop ? termTop : termBot;
        const float dx = legEnd.x - legStart.x;
        const float dy = legEnd.y - legStart.y;
        const float len = std::hypot(dx, dy);
        if (len > 0.1f) {
            const float dirSign = isPNP ? -1.0f : 1.0f;
            const float ux = (dx / len) * dirSign;
            const float uy = (dy / len) * dirSign;
            const float nx = -uy;
            const float ny = ux;

            const Vec2 mid{
                (legStart.x + legEnd.x) * 0.5f + ux * 2.2f,
                (legStart.y + legEnd.y) * 0.5f + uy * 2.2f
            };
            const Vec2 a1{mid.x - ux * 5.0f + nx * 2.8f, mid.y - uy * 5.0f + ny * 2.8f};
            const Vec2 a2{mid.x - ux * 5.0f - nx * 2.8f, mid.y - uy * 5.0f - ny * 2.8f};
            m_scene.DrawTriangle(mid, a1, a2, m_pal.inkBold);
        }

        if (labelSide == -2) {
            // Place both lines above-left of the horizontal base lead
            SideLabel({center.x - r - 2.0f, center.y - 12.5f}, -1, refDes, partNum);
            return;
        }
        const int side = (labelSide != 0) ? labelSide : (baseLeft ? 1 : -1);
        const float ax = (side > 0) ? (center.x + r + 3.5f) : (center.x - r - 3.5f);
        const bool onBaseSide = (baseLeft && side < 0) || (!baseLeft && side > 0);
        if (onBaseSide) {
            // Split refDes above the horizontal base wire and partNum below the base wire so the wire never crosses text!
            const float boxW = 48.0f;
            const float alignX = (side >= 0) ? 0.0f : 1.0f;
            const float rx = (side >= 0) ? ax : (ax - boxW);
            if (!refDes.empty()) {
                m_scene.DrawTextBox(refDes, Rect(rx, center.y - 13.0f, boxW, 8.5f), 6.0f, m_pal.inkBold, alignX, 0.5f, FontStyle::Bold);
            }
            if (!partNum.empty()) {
                m_scene.DrawTextBox(partNum, Rect(rx, center.y + 4.0f, boxW, 8.5f), 5.6f, m_pal.ink, alignX, 0.5f, FontStyle::Bold);
            }
        } else {
            SideLabel({ax, center.y}, side, refDes, partNum);
        }
    }

    /// Draws the diamond full-wave bridge rectifier (`MR1`–`MR4`, `1S421 / PM7A2L`).
    /// Corners: Top `(cx, cy-r)`, Right `(cx+r, cy)`, Bottom `(cx, cy+r)`, Left `(cx-r, cy)`.
    void DiodeBridge(Vec2 center, float r = 44.0f) {
        auto item = m_scene.Item({
            .id = "MR1..MR4",
            .title = "1S421 / PM7A2L",
            .category = "Bridge Rectifier"
        });
        const Vec2 top{center.x, center.y - r};
        const Vec2 right{center.x + r, center.y};
        const Vec2 bot{center.x, center.y + r};
        const Vec2 left{center.x - r, center.y};

        Wire(left, top);
        Wire(right, top);
        Wire(bot, left);
        Wire(bot, right);

        Junction(top);
        Junction(right);
        Junction(bot);
        Junction(left);

        // 4 diodes along the diamond edges conducting toward Top (+DC) from Bottom (-DC)
        Diode({(left.x + top.x) * 0.5f,  (left.y + top.y) * 0.5f},  -45.0f,  "", "", false, -1);
        Diode({(right.x + top.x) * 0.5f, (right.y + top.y) * 0.5f}, -135.0f, "", "", false,  1);
        Diode({(bot.x + left.x) * 0.5f,  (bot.y + left.y) * 0.5f},  -135.0f, "", "", false, -1);
        Diode({(bot.x + right.x) * 0.5f, (bot.y + right.y) * 0.5f}, -45.0f,  "", "", false,  1);

        // Labels positioned cleanly outside each of the 4 diamond edges
        SideLabel({center.x - r * 0.58f - 8.0f, center.y - r * 0.55f}, -1, "MR1", "1S421");
        SideLabel({center.x + r * 0.58f + 8.0f, center.y - r * 0.55f},  1, "MR2", "1S421");
        SideLabel({center.x - r * 0.58f - 8.0f, center.y + r * 0.55f}, -1, "MR3", "1S421R");
        SideLabel({center.x + r * 0.58f + 8.0f, center.y + r * 0.55f},  1, "MR4", "1S421R");

        m_scene.DrawTextBox(
            "OR",
            Rect(center.x - 24.0f, center.y - 8.5f, 48.0f, 8.0f),
            5.5f,
            m_pal.inkBold,
            0.5f,
            0.5f,
            FontStyle::Bold
        );
        m_scene.DrawTextBox(
            "PM7A2L",
            Rect(center.x - 28.0f, center.y - 0.5f, 56.0f, 8.5f),
            6.0f,
            m_pal.inkBold,
            0.5f,
            0.5f,
            FontStyle::Bold
        );
    }

    // =========================================================================
    // 4. CONNECTORS, TRANSFORMER, PCB LAYOUTS & QUAD TITLE BLOCK
    // =========================================================================

    /// Draws the 4-pin DIN input socket `SK1` and its off-sheet dual routing at the far left.
    void DinSocketSK1(Vec2 center, float yIn, float yChs, float xDriverLeft) {
        const Rect shield(center.x - 20.0f, center.y - 26.0f, 40.0f, 56.0f);
        constexpr float r = 16.5f;

        // 4 internal pins: 1 (top-left), 3 (top-right), 2 (bottom-left), 4 (bottom-right)
        const Vec2 p1{center.x - 5.5f, center.y - 6.0f};
        const Vec2 p3{center.x + 5.5f, center.y - 6.0f};
        const Vec2 p2{center.x - 6.0f, center.y + 5.5f};
        const Vec2 p4{center.x + 6.0f, center.y + 5.5f};

        // Pin 1 -> up to yIn -> right to Pin 10
        WireV(p1.x, yIn, p1.y);
        WireH(p1.x, xDriverLeft, yIn);

        // Pin 3 -> up above shield -> left around SK1 -> down to Righthand Amplifier arrow 1
        const float yTopLoop = yIn + 14.0f;
        const float xLeftOuter = 24.0f;
        WireV(p3.x, yTopLoop, p3.y);
        WireH(xLeftOuter, p3.x, yTopLoop);
        WireV(xLeftOuter, yTopLoop, yChs);
        WireH(xLeftOuter, 60.0f, yChs);
        WireV(60.0f, yChs, 765.0f);

        // Pin 2 -> left to grounded shield wall + down to yChs return rail (Pin 11)
        WireH(shield.x, p2.x, p2.y);
        Junction({shield.x, p2.y});
        WireV(shield.x, p2.y, center.y + 42.0f);
        ChassisEarth({shield.x, center.y + 42.0f});

        // Second vertical line to Righthand Amplifier arrow 2 (shielded return)
        WireV(76.0f, center.y + 30.0f, 765.0f);
        WireH(shield.x, xDriverLeft, yChs);
        Junction({76.0f, yChs});

        {
            auto sk1Item = m_scene.Item({
                .id = "SK1",
                .title = "4-Pin DIN Input",
                .category = "Connector",
                .hitRect = shield
            });
            DashedRect(shield, 5.5f, 3.5f, 1.15f);
            m_scene.DrawText("SK1", {shield.x - 18.0f, center.y - 22.0f}, 6.0f, m_pal.inkBold, FontStyle::Bold);
            m_scene.DrawCircle(center, r, m_pal.bodyFill, m_pal.inkBold, m_pal.symbolWidth);
            SolderPad(p1, 2.2f);
            SolderPad(p2, 2.2f);
            SolderPad(p3, 2.2f);
            SolderPad(p4, 2.2f);
            m_scene.DrawText("1", {p1.x - 6.5f, p1.y - 2.0f}, 5.2f, m_pal.inkBold, FontStyle::Bold);
            m_scene.DrawText("3", {p3.x + 3.2f, p3.y - 2.0f}, 5.2f, m_pal.inkBold, FontStyle::Bold);
            m_scene.DrawText("2", {p2.x - 6.5f, p2.y - 2.0f}, 5.2f, m_pal.inkBold, FontStyle::Bold);
            m_scene.DrawText("4", {p4.x + 3.2f, p4.y - 2.0f}, 5.2f, m_pal.inkBold, FontStyle::Bold);
        }

        // Dual arrows at bottom-left ("TO RIGHTHAND AMPLIFIER")
        m_scene.DrawTriangle({60.0f, 765.0f}, {56.6f, 758.0f}, {63.4f, 758.0f}, m_pal.bodyFill, m_pal.inkBold, 1.2f);
        m_scene.DrawTriangle({76.0f, 765.0f}, {72.6f, 758.0f}, {79.4f, 758.0f}, m_pal.bodyFill, m_pal.inkBold, 1.2f);
        m_scene.DrawTextBox("TO RIGHTHAND", Rect(20.0f, 769.0f, 96.0f, 8.0f), 5.6f, m_pal.inkBold, 0.5f, 0.5f, FontStyle::Bold);
        m_scene.DrawLine({38.0f, 778.5f}, {98.0f, 778.5f}, m_pal.ink, 0.85f);
        m_scene.DrawTextBox("AMPLIFIER", Rect(20.0f, 780.0f, 96.0f, 8.0f), 5.6f, m_pal.inkBold, 0.5f, 0.5f, FontStyle::Bold);
        m_scene.DrawLine({46.0f, 789.5f}, {90.0f, 789.5f}, m_pal.ink, 0.85f);
    }

    /// Draws the Mains Transformer `T1`, voltage tap selector, neon lamp `V1`, fuse `FS1`, and mains plug `PL1`.
    void MainsTransformerSection(Vec2 topLeft, float ySecTop, float ySecBot) {
        const float tx = topLeft.x;
        const float ty = topLeft.y;
        const float coreTop = ty + 4.0f;
        const float coreBot = ty + 292.0f;
        const float priX = tx + 12.0f;
        const float priR = 6.2f;
        const int secLoops = 8;
        const float secR = (ySecBot - ySecTop) / (static_cast<float>(secLoops) * 2.0f);

        {
            auto t1Item = m_scene.Item({
                .id = "T1",
                .title = "Mains Power Transformer",
                .category = "Transformer"
            });
            m_scene.DrawText("T1", {tx - 4.0f, ty - 16.0f}, 6.6f, m_pal.inkBold, FontStyle::Bold);

            // Secondary winding (left of core, loops bulging LEFT: 90 deg sweep 180 deg)
            // Includes 8 active loops (ySecTop..ySecBot) + 3 lower loops with open tap tail
            for (int i = 0; i < secLoops + 3; ++i) {
                const float cy = ySecTop + secR * (2.0f * static_cast<float>(i) + 1.0f);
                m_scene.DrawArc({tx, cy}, secR, 90.0f, 180.0f, m_pal.inkBold, m_pal.symbolWidth, 9);
            }
            WireH(tx - 12.0f, tx, ySecBot + secR * 6.0f);

            // Dual iron core vertical bars
            m_scene.DrawLine({tx + 4.0f, coreTop}, {tx + 4.0f, coreBot}, m_pal.inkBold, 1.9f);
            m_scene.DrawLine({tx + 8.0f, coreTop}, {tx + 8.0f, coreBot}, m_pal.inkBold, 1.9f);

            // Two primary windings (right of core, loops bulging RIGHT: -90 deg sweep 180 deg)
            // Upper primary winding (9 loops: y = ty+10 .. ty+122)
            for (int i = 0; i < 9; ++i) {
                m_scene.DrawArc({priX, ty + 10.0f + priR * (2.0f * i + 1.0f)}, priR, -90.0f, 180.0f, m_pal.inkBold, m_pal.symbolWidth, 8);
            }
            // Lower primary winding (11 loops: y = ty+148 .. ty+284)
            for (int i = 0; i < 11; ++i) {
                m_scene.DrawArc({priX, ty + 148.0f + priR * (2.0f * i + 1.0f)}, priR, -90.0f, 180.0f, m_pal.inkBold, m_pal.symbolWidth, 8);
            }
        }

        // Voltage Tap Selector Rotary Carousel
        const Vec2 selCenter{tx + 72.0f, ty + 114.0f};
        Vec2 pads[6];
        {
            auto vsItem = m_scene.Item({
                .id = "VS1",
                .title = "100-250V Tap Selector",
                .category = "Selector"
            });
            m_scene.DrawCircle(selCenter, 20.0f, m_pal.bodyFill, m_pal.inkBold, m_pal.symbolWidth);
            for (int p = 0; p < 6; ++p) {
                const float ang = (static_cast<float>(p) * 60.0f - 90.0f) * (3.14159265f / 180.0f);
                pads[p] = {selCenter.x + std::cos(ang) * 13.0f, selCenter.y + std::sin(ang) * 13.0f};
                SolderPad(pads[p], 2.3f);
            }
            // Internal selector bridges
            m_scene.DrawLine(pads[4], pads[1], m_pal.inkBold, 1.8f);
            m_scene.DrawLine(pads[3], pads[2], m_pal.inkBold, 1.8f);
        }

        // Primary tap connections matching M12160
        const float yPri1Top = ty + 10.0f;
        const float yPri1Tap = ty + 72.0f;
        const float yPri1Bot = ty + 121.6f;
        const float yPri2Top = ty + 148.0f;
        const float yPri2Tap = ty + 185.0f;
        const float yPri2Bot = ty + 284.4f;

        // Top of upper primary -> goes right to x=tx+114 -> down to Fuse FS1
        const float xRightMains = tx + 114.0f;
        WireH(priX, xRightMains, yPri1Top);
        WireV(xRightMains, yPri1Top, ty + 258.0f);

        // Tap of upper primary -> selector top pad
        WireH(priX, selCenter.x, yPri1Tap);
        WireV(selCenter.x, yPri1Tap, pads[0].y - 2.3f);

        // Bottom of upper primary -> selector upper-left pad
        WireH(priX, pads[5].x - 2.3f, yPri1Bot);

        // Top of lower primary -> selector lower-left pad + neon indicator column
        const float neonX = tx + 42.0f;
        WireH(priX, pads[4].x, yPri2Top);
        WireV(pads[4].x, pads[4].y + 2.3f, yPri2Top);
        Junction({neonX, yPri2Top});

        // Tap of lower primary -> goes right to x=tx+96 -> up into selector right pad
        WireH(priX, tx + 96.0f, yPri2Tap);
        WireV(tx + 96.0f, pads[1].y, yPri2Tap);
        WireH(pads[1].x + 2.3f, tx + 96.0f, pads[1].y);

        // Selector bottom-right pad -> goes down to xRightMains
        WireV(pads[2].x, pads[2].y + 2.3f, ty + 162.0f);
        WireH(pads[2].x, xRightMains, ty + 162.0f);
        Junction({xRightMains, ty + 162.0f});

        // Selector bottom pad -> goes straight down as Left Mains line to PL1
        const float xLeftMains = selCenter.x;
        WireV(xLeftMains, pads[3].y + 2.3f, ty + 356.0f);

        // Bottom of lower primary -> goes right to xLeftMains
        WireH(priX, xLeftMains, yPri2Bot);
        Junction({xLeftMains, yPri2Bot});

        // Neon indicator branch (R2 100K + V1 SGL) between yPri2Top and yPri2Bot
        WireV(neonX, yPri2Top, yPri2Bot);
        Junction({neonX, yPri2Bot});
        Resistor({neonX, ty + 196.0f}, true, "R2", "100K", 1);
        const Vec2 v1Pos{neonX, ty + 252.0f};
        {
            auto v1Item = m_scene.Item({
                .id = "V1",
                .title = "SGL Neon Indicator",
                .category = "Indicator"
            });
            m_scene.DrawCircle(v1Pos, 8.5f, m_pal.bodyFill, m_pal.inkBold, m_pal.symbolWidth);
            SolderPad({v1Pos.x - 2.6f, v1Pos.y}, 1.6f);
            SolderPad({v1Pos.x + 2.6f, v1Pos.y}, 1.6f);
            Junction({v1Pos.x - 2.6f, v1Pos.y + 4.2f}, 1.3f);
            SideLabel({v1Pos.x + 10.5f, v1Pos.y}, 1, "V1", "SGL");
        }

        // Fuse FS1 on Right Mains line (steps inward to xFuse = tx + 92)
        const float xFuse = tx + 92.0f;
        WireH(xFuse, xRightMains, ty + 258.0f);
        WireV(xFuse, ty + 258.0f, ty + 356.0f);
        Fuse({xFuse, ty + 292.0f}, true, "FS1", 1);

        // PL1 3-pin Mains Connector Circle
        const Vec2 pl1Pos{(xLeftMains + xFuse) * 0.5f, ty + 360.0f};
        {
            auto pl1Item = m_scene.Item({
                .id = "PL1",
                .title = "3-Pin Mains Input",
                .category = "Connector"
            });
            m_scene.DrawCircle(pl1Pos, 17.5f, m_pal.bodyFill, m_pal.inkBold, m_pal.symbolWidth);
            Junction({xLeftMains, pl1Pos.y - 3.0f}, 2.6f);
            Junction({xFuse,      pl1Pos.y - 3.0f}, 2.6f);
            Junction({pl1Pos.x,   pl1Pos.y + 8.5f}, 2.6f);
            m_scene.DrawText("PL1", {pl1Pos.x + 20.0f, pl1Pos.y + 4.0f}, 6.2f, m_pal.inkBold, FontStyle::Bold);
        }

        // Dashed electrostatic screen / core ground line from T1 core to PL1 Earth pin
        WireV(pl1Pos.x, pl1Pos.y + 8.5f, pl1Pos.y + 24.0f);
        ChassisEarth({pl1Pos.x, pl1Pos.y + 24.0f});
        DashedLine({tx + 4.0f, coreBot}, {tx + 4.0f, pl1Pos.y + 20.0f}, 5.5f, 3.5f, 1.15f);
        DashedLine({tx + 4.0f, pl1Pos.y + 20.0f}, {pl1Pos.x, pl1Pos.y + 20.0f}, 5.5f, 3.5f, 1.15f);
        Junction({pl1Pos.x, pl1Pos.y + 20.0f}, 2.2f);
    }

    /// Draws the authentic right-hand mechanical PCB outlines (`M12098 ISS. 10` and `M12088 ISS. 5`).
    void PcbCopperOutlines(Rect topBoard, Rect botBoard) {
        // 1. Top Board: Driver Board M12098 ISS. 10 with stepped left corners and right edge notch
        m_scene.DrawRect(topBoard, m_pal.bodyFill);
        const float x0 = topBoard.x;
        const float y0 = topBoard.y;
        const float x1 = topBoard.Right();
        const float y1 = topBoard.Bottom();
        const float stepX = 10.0f;
        const float stepY = 46.0f;
        const float notchX = 12.0f;
        const float notchY1 = y0 + 52.0f;
        const float notchY2 = y1 - 52.0f;

        // Stepped polygon outline of M12098 board
        WireH(x0, x1, y0);
        WireV(x0, y0, y0 + stepY);
        WireH(x0, x0 + stepX, y0 + stepY);
        WireV(x0 + stepX, y0 + stepY, y1 - stepY);
        WireH(x0, x0 + stepX, y1 - stepY);
        WireV(x0, y1 - stepY, y1);
        WireH(x0, x1, y1);
        WireV(x1, y0, notchY1);
        WireH(x1 - notchX, x1, notchY1);
        WireV(x1 - notchX, notchY1, notchY2);
        WireH(x1 - notchX, x1, notchY2);
        WireV(x1, notchY2, y1);

        // RV100 and RV101 holes
        SolderPad({x0 + 48.0f, y0 + 28.0f}, 3.4f);
        m_scene.DrawTextBox("RV100", Rect(x0 + 24.0f, y0 + 13.0f, 48.0f, 9.0f), 6.0f, m_pal.inkBold, 0.5f, 0.5f, FontStyle::Bold);

        SolderPad({x0 + 124.0f, y0 + 78.0f}, 3.4f);
        m_scene.DrawTextBox("RV101", Rect(x0 + 100.0f, y0 + 63.0f, 48.0f, 9.0f), 6.0f, m_pal.inkBold, 0.5f, 0.5f, FontStyle::Bold);

        // Left pins 10 and 11 (two circles side by side with 10 and 11 above them)
        SolderPad({x0 + 30.0f, y0 + 142.0f}, 2.8f);
        SolderPad({x0 + 48.0f, y0 + 142.0f}, 2.8f);
        m_scene.DrawTextBox("10", Rect(x0 + 20.0f, y0 + 128.0f, 20.0f, 9.0f), 5.8f, m_pal.inkBold, 0.5f, 0.5f, FontStyle::Bold);
        m_scene.DrawTextBox("11", Rect(x0 + 38.0f, y0 + 128.0f, 20.0f, 9.0f), 5.8f, m_pal.inkBold, 0.5f, 0.5f, FontStyle::Bold);

        // Right edge solder pads 1..9
        const char* padNums9[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
        const float padYs9[] = {36.0f, 62.0f, 80.0f, 98.0f, 116.0f, 134.0f, 152.0f, 178.0f, 196.0f};
        for (int i = 0; i < 9; ++i) {
            const float py = y0 + padYs9[i];
            SolderPad({x1 - 24.0f, py}, 2.6f);
            m_scene.DrawTextBox(padNums9[i], Rect(x1 - 44.0f, py - 4.5f, 15.0f, 9.0f), 6.0f, m_pal.inkBold, 1.0f, 0.5f, FontStyle::Bold);
        }

        m_scene.DrawTextBox(
            "M 12098   ISS. 10",
            Rect(x0 + 40.0f, y0 + 148.0f, topBoard.w - 80.0f, 14.0f),
            6.8f,
            m_pal.inkBold,
            0.5f,
            0.5f,
            FontStyle::Bold
        );

        // 2. Bottom Board: Power Supply Board M12088 ISS. 5
        m_scene.DrawRect(botBoard, m_pal.bodyFill, 2.0f, m_pal.inkBold, m_pal.symbolWidth);
        SolderPad({botBoard.x + 34.0f, botBoard.Bottom() - 30.0f}, 3.4f);
        m_scene.DrawTextBox("RV200", Rect(botBoard.x + 12.0f, botBoard.Bottom() - 22.0f, 44.0f, 9.0f), 6.0f, m_pal.inkBold, 0.5f, 0.5f, FontStyle::Bold);

        const char* padNums4[] = {"12", "13", "14", "15"};
        for (int i = 0; i < 4; ++i) {
            const float py = botBoard.y + 38.0f + static_cast<float>(i) * 16.0f;
            SolderPad({botBoard.Right() - 28.0f, py}, 2.6f);
            m_scene.DrawTextBox(padNums4[i], Rect(botBoard.Right() - 22.0f, py - 4.5f, 18.0f, 9.0f), 6.0f, m_pal.inkBold, 0.0f, 0.5f, FontStyle::Bold);
        }

        m_scene.DrawTextBox(
            "M 12088   ISS. 5",
            Rect(botBoard.x + 30.0f, botBoard.y + 52.0f, botBoard.w - 70.0f, 14.0f),
            6.8f,
            m_pal.inkBold,
            0.5f,
            0.5f,
            FontStyle::Bold
        );

        // Caption underneath both boards
        const float capY = botBoard.Bottom() + 34.0f;
        m_scene.DrawTextBox(
            "BOARDS  VIEWED  FROM  COPPER  SIDE",
            Rect(botBoard.x, capY, botBoard.w, 10.0f),
            6.4f,
            m_pal.inkBold,
            0.5f,
            0.5f,
            FontStyle::Bold
        );
        m_scene.DrawLine({botBoard.x + 38.0f, capY + 12.0f}, {botBoard.Right() - 38.0f, capY + 12.0f}, m_pal.inkBold, 0.95f);
    }

    /// Draws the authentic bottom-right engineering title block:
    /// "QUAD — The Acoustical Mfg. Co. Ltd. Huntingdon England | QUAD 303 CIRCUIT DIAGRAM | M12160".
    void QuadTitleBlock(Rect box) {
        m_scene.DrawRect(box, m_pal.bodyFill, 0.0f, m_pal.inkBold, m_pal.symbolWidth * 1.2f);

        const float midX = box.x + box.w * 0.55f;
        const float revCol1 = box.x + 38.0f;
        const float revCol2 = box.x + 84.0f;
        const float signCol = midX - 118.0f;

        // Vertical dividers
        m_scene.DrawLine({revCol1, box.y}, {revCol1, box.Bottom()}, m_pal.ink, 1.0f);
        m_scene.DrawLine({revCol2, box.y}, {revCol2, box.Bottom()}, m_pal.ink, 1.0f);
        m_scene.DrawLine({signCol, box.y}, {signCol, box.Bottom()}, m_pal.ink, 1.0f);
        m_scene.DrawLine({midX,    box.y}, {midX,    box.Bottom()}, m_pal.inkBold, 1.4f);

        // Left Revision Table horizontal rows
        const float rh = box.h / 6.0f;
        for (int r = 1; r <= 5; ++r) {
            const float ry = box.y + static_cast<float>(r) * rh;
            m_scene.DrawLine({box.x, ry}, {midX, ry}, m_pal.ink, 0.9f);
        }

        m_scene.DrawText("Issue",        {box.x + 6.0f,    box.y + 4.0f}, 6.0f, m_pal.inkBold, FontStyle::Bold);
        m_scene.DrawText("Date",         {revCol1 + 8.0f,  box.y + 4.0f}, 6.0f, m_pal.inkBold, FontStyle::Bold);
        m_scene.DrawText("Modification", {revCol2 + 24.0f, box.y + 4.0f}, 6.0f, m_pal.inkBold, FontStyle::Bold);

        m_scene.DrawText("No. 2.",       {box.x + 6.0f,    box.y + rh + 4.0f}, 5.8f, m_pal.inkBold, FontStyle::Bold);
        m_scene.DrawText("4-9-70",       {revCol1 + 5.0f,  box.y + rh + 4.0f}, 5.8f, m_pal.ink);
        m_scene.DrawText("REDRAWN",      {revCol2 + 8.0f,  box.y + rh + 4.0f}, 5.8f, m_pal.ink);

        m_scene.DrawText("No. 3",        {box.x + 6.0f,    box.y + rh * 2.0f + 4.0f}, 5.8f, m_pal.inkBold, FontStyle::Bold);
        m_scene.DrawText("12-11-73",     {revCol1 + 5.0f,  box.y + rh * 2.0f + 4.0f}, 5.8f, m_pal.ink);

        // Drawn / Traced / Checked / Approved sub-table with initials & dates
        const float sigSplit1 = signCol + 48.0f;
        const float sigSplit2 = signCol + 78.0f;
        m_scene.DrawLine({sigSplit1, box.y + rh * 2.0f}, {sigSplit1, box.Bottom()}, m_pal.ink, 0.9f);
        m_scene.DrawLine({sigSplit2, box.y + rh * 2.0f}, {sigSplit2, box.Bottom()}, m_pal.ink, 0.9f);

        m_scene.DrawText("Drawn",    {signCol + 5.0f, box.y + rh * 2.0f + 4.0f}, 5.6f, m_pal.ink);
        m_scene.DrawText("Traced",   {signCol + 5.0f, box.y + rh * 3.0f + 4.0f}, 5.6f, m_pal.ink);
        m_scene.DrawText("Checked",  {signCol + 5.0f, box.y + rh * 4.0f + 4.0f}, 5.6f, m_pal.ink);
        m_scene.DrawText("Approved", {signCol + 5.0f, box.y + rh * 5.0f + 4.0f}, 5.6f, m_pal.ink);

        m_scene.DrawText("R.W.", {sigSplit1 + 5.0f, box.y + rh * 2.0f + 4.0f}, 5.6f, m_pal.inkBold, FontStyle::Bold);
        m_scene.DrawText("R.W.", {sigSplit1 + 5.0f, box.y + rh * 3.0f + 4.0f}, 5.6f, m_pal.inkBold, FontStyle::Bold);
        m_scene.DrawText("J.B.", {sigSplit1 + 5.0f, box.y + rh * 4.0f + 4.0f}, 5.6f, m_pal.inkBold, FontStyle::Bold);

        m_scene.DrawText("5-3-70",  {sigSplit2 + 4.0f, box.y + rh * 2.0f + 4.0f}, 5.2f, m_pal.ink);
        m_scene.DrawText("6-3-70",  {sigSplit2 + 4.0f, box.y + rh * 3.0f + 4.0f}, 5.2f, m_pal.ink);
        m_scene.DrawText("11-3-70", {sigSplit2 + 4.0f, box.y + rh * 4.0f + 4.0f}, 5.2f, m_pal.ink);

        // Right Title Area
        const float titleSplitY = box.y + box.h * 0.42f;
        const float dwgNoX = box.Right() - 96.0f;
        m_scene.DrawLine({midX, titleSplitY}, {box.Right(), titleSplitY}, m_pal.inkBold, 1.2f);
        m_scene.DrawLine({dwgNoX, titleSplitY}, {dwgNoX, box.Bottom()}, m_pal.inkBold, 1.2f);

        m_scene.DrawText("QUAD —", {midX + 10.0f, box.y + 11.0f}, 11.0f, m_pal.inkBold, FontStyle::Bold);
        m_scene.DrawText(
            "The Acoustical Mfg. Co. Ltd.   Huntingdon   England.",
            {midX + 66.0f, box.y + 14.0f},
            7.2f,
            m_pal.ink,
            FontStyle::Italic
        );

        m_scene.DrawText("QUAD  303", {midX + 12.0f, titleSplitY + 7.0f}, 10.5f, m_pal.inkBold, FontStyle::Bold);
        m_scene.DrawText("CIRCUIT  DIAGRAM", {midX + 12.0f, titleSplitY + 22.0f}, 9.5f, m_pal.inkBold, FontStyle::Bold);
        m_scene.DrawText("(ABOVE SERIAL No. 11600)", {midX + 12.0f, titleSplitY + 37.0f}, 7.2f, m_pal.inkBold, FontStyle::Bold);

        m_scene.DrawTextBox(
            "M 12160",
            Rect(dwgNoX, titleSplitY, box.Right() - dwgNoX, box.Bottom() - titleSplitY),
            10.5f,
            m_pal.inkBold,
            0.5f,
            0.55f,
            FontStyle::Bold
        );
    }

private:
    SceneScope& m_scene;
    SchematicPalette m_pal;
};

/// Renders the complete QUAD 303 Power Amplifier Circuit Diagram (Drawing M12160)
/// in `SceneScope` World Coordinates (2200 x 920 units).
inline void RenderQuad303Schematic(SceneScope& scene, const SchematicPalette& pal) {
    SchematicPainter sp(scene, pal);

    // Outer drafting sheet border
    scene.DrawRect(Rect(10.0f, 10.0f, 2180.0f, 900.0f), pal.sheetBg, 4.0f, pal.inkMuted, 1.0f);

    // =========================================================================
    // GLOBAL HORIZONTAL RAILS & BOARD BOUNDARIES
    // =========================================================================
    const float yRail67V = 52.0f;    // Top +67V Regulated Supply Rail (Pin 1 / Pin 12)
    const float yRailGnd = 690.0f;   // Main Amplifier 0V / Negative Return Rail (Pin 8)
    const float yRailChs = 716.0f;   // Chassis / Signal Return Rail (Pin 9 / Pin 11)

    // 1. LEFTHAND AMPLIFIER — DRIVER BOARD M12098 Dashed Enclosure
    const Rect driverBox(118.0f, 24.0f, 742.0f, 704.0f);
    sp.DashedRect(driverBox, 10.0f, 5.0f, 1.4f);

    scene.DrawText("LEFTHAND AMPLIFIER", {142.0f, 180.0f}, 6.6f, pal.inkBold, FontStyle::Bold);
    scene.DrawLine({142.0f, 190.5f}, {244.0f, 190.5f}, pal.ink, 0.9f);
    scene.DrawText("DRIVER BOARD", {142.0f, 242.0f}, 6.4f, pal.inkBold, FontStyle::Bold);
    scene.DrawLine({142.0f, 252.5f}, {210.0f, 252.5f}, pal.ink, 0.9f);
    scene.DrawText("M 12098", {148.0f, 257.0f}, 6.4f, pal.inkBold, FontStyle::Bold);
    scene.DrawLine({148.0f, 267.5f}, {190.0f, 267.5f}, pal.ink, 0.9f);

    // Top +67V supply line from Bridge Top (x=1605) across to R104 (x=306)
    sp.WireH(306.0f, 1605.0f, yRail67V);
    scene.DrawText("67 V", {872.0f, yRail67V - 13.0f}, 6.6f, pal.accent, FontStyle::Bold);

    // Off-sheet branch to Righthand Amplifier from +67V rail
    sp.WireV(956.0f, yRail67V, 92.0f);
    sp.Junction({956.0f, yRail67V});
    sp.SignalArrowDown({956.0f, 92.0f}, "TO RIGHTHAND", "AMPLIFIER");

    // Bottom rails (Pin 8 & Pin 9)
    sp.WireH(306.0f, 924.0f, yRailGnd);
    sp.WireH(driverBox.x, 962.0f, yRailChs);

    // =========================================================================
    // INPUT STAGE (SK1, C100, R100, R101, TR100, TR101, C101..C105, RV100)
    // =========================================================================
    const float yIn = 492.0f;
    sp.DinSocketSK1({68.0f, 540.0f}, yIn, yRailChs, driverBox.x);

    // Input signal line from Pin 10 (x=118, y=492) to TR100 base (x=291)
    sp.WireH(driverBox.x, 291.0f, yIn);

    // R100 (220K) input shunt to Pin 11 chassis rail
    sp.WireV(144.0f, yIn, yRailChs);
    sp.Junction({144.0f, yIn});
    sp.Junction({144.0f, yRailChs});
    sp.Resistor({144.0f, 576.0f}, true, "R100", "220K", 1);

    // C100 (.68µ) & R101 (2.2K) in series
    sp.Capacitor({174.0f, yIn}, false, "C100", ".68u", false, -1, -1);
    sp.Resistor({212.0f, yIn}, false, "R101", "2.2K", -1);

    // Feedback tap after R101 (x=236) up to C105 (100p) || R108 (22K) loop (y=386)
    sp.WireV(236.0f, 386.0f, yIn);
    sp.Junction({236.0f, yIn});
    sp.WireH(236.0f, 336.0f, 386.0f);

    // TR100 (BC154) PNP Input Transistor at (306, 492)
    const Vec2 tr100Pos{306.0f, yIn};
    sp.TransistorBJT(tr100Pos, true, "TR100", "BC 154", true, true, 1);

    // R104 (22K) from +67V rail down to TR100 emitter (y=477)
    sp.WireV(306.0f, yRail67V, tr100Pos.y - 15.0f);
    sp.Resistor({306.0f, 185.0f}, true, "R104", "22K", 1);
    scene.DrawText("9V APPROX.", {312.0f, 272.0f}, 5.8f, pal.accent, FontStyle::Bold);

    // Emitter AC/DC network: from TR100 emitter (y=452) left to x=258, down to C101 (47µ) || R103 (1K)
    sp.WireH(258.0f, 306.0f, 452.0f);
    sp.Junction({306.0f, 452.0f});
    sp.WireV(258.0f, 452.0f, 592.0f);
    // Parallel loop: C101 (47µ) at x=234 and R103 (1K) at x=280 (y=592..656)
    sp.WireH(234.0f, 280.0f, 592.0f);
    sp.WireH(234.0f, 280.0f, 656.0f);
    sp.WireV(234.0f, 592.0f, 656.0f);
    sp.WireV(280.0f, 592.0f, 656.0f);
    sp.Junction({258.0f, 592.0f});
    sp.Junction({258.0f, 656.0f});
    sp.Capacitor({234.0f, 624.0f}, true, "C101", "47u", true, -1, -1);
    sp.Resistor({280.0f, 624.0f}, true, "R103", "1K", -1);
    sp.WireV(258.0f, 656.0f, yRailChs);
    sp.Junction({258.0f, yRailChs});

    // Collector of TR100 (x=306, y=507) -> C103 (1000p) || R105 (47K) -> TR101 base (y=592) -> R106 (22K) -> C102 (.01µ) -> Pin 11 rail
    sp.WireV(306.0f, tr100Pos.y + 15.0f, 520.0f);
    sp.WireH(292.0f, 320.0f, 520.0f);
    sp.WireH(292.0f, 320.0f, 578.0f);
    sp.WireV(292.0f, 520.0f, 578.0f);
    sp.WireV(320.0f, 520.0f, 578.0f);
    sp.Junction({306.0f, 520.0f});
    sp.Junction({306.0f, 578.0f});
    sp.Capacitor({292.0f, 549.0f}, true, "C103", "1000p", false, -1, -1);
    sp.Resistor({320.0f, 549.0f}, true, "R105", "47K", 1);

    sp.WireV(306.0f, 578.0f, yRailChs);
    sp.Junction({306.0f, yRailGnd});
    sp.Junction({306.0f, yRailChs});
    sp.Resistor({306.0f, 626.0f}, true, "R106", "22K", 1);
    sp.Capacitor({306.0f, 672.0f}, true, "C102", ".01u", false, -1, -1);

    // TR101 (BC109) NPN Second Stage at (380, 592)
    const Vec2 tr101Pos{380.0f, 592.0f};
    sp.WireH(306.0f, tr101Pos.x - 15.0f, 592.0f);
    sp.Junction({306.0f, 592.0f});
    // R107 (150K) from TR101 base to yRailChs
    sp.WireV(342.0f, 592.0f, yRailChs);
    sp.Junction({342.0f, 592.0f});
    sp.Junction({342.0f, yRailChs});
    sp.Resistor({342.0f, 650.0f}, true, "R107", "150K", 1);

    sp.TransistorBJT(tr101Pos, false, "TR101", "BC 109", true, false, -2);
    sp.WireV(380.0f, tr101Pos.y + 15.0f, yRailChs);
    sp.Junction({380.0f, yRailChs});
    sp.Resistor({380.0f, 650.0f}, true, "R109", "1.5K", 1);

    // C105 (100p) in parallel with R108 (22K) feedback network at y=364..408
    sp.WireH(336.0f, 384.0f, 364.0f);
    sp.WireH(336.0f, 384.0f, 408.0f);
    sp.WireV(336.0f, 364.0f, 408.0f);
    sp.WireV(384.0f, 364.0f, 408.0f);
    sp.Junction({336.0f, 386.0f});
    sp.Junction({384.0f, 386.0f});
    sp.Capacitor({360.0f, 364.0f}, false, "C105", "100p", false, -1, -1);
    sp.Resistor({360.0f, 408.0f}, false, "R108", "22K", -1);
    // Feedback line from right of C105||R108 (x=384, y=386) across to C110 node (x=558, y=386)
    sp.WireH(384.0f, 558.0f, 386.0f);
    scene.DrawText("0.6V APPROX.", {408.0f, 375.0f}, 5.6f, pal.accent, FontStyle::Bold);

    // R110 (56K) column (x=404) + RV100 (4.7K) + R110B (4.7K) + C104 (10µ) / R111 (18K)
    sp.WireV(404.0f, yRail67V, 560.0f);
    sp.Junction({404.0f, yRail67V});
    sp.Resistor({404.0f, 185.0f}, true, "R110", "56K", -1);
    sp.Potentiometer({404.0f, 482.0f}, true, "RV100", "4.7K", -1);
    sp.Resistor({404.0f, 534.0f}, true, "R109", "4.7K", -1);

    // R111 (18K) & C104 (10µ) branch at x=442
    sp.WireV(442.0f, 386.0f, yRailChs);
    sp.Junction({442.0f, 386.0f});
    sp.Junction({442.0f, yRailChs});
    sp.Resistor({442.0f, 424.0f}, true, "R111", "18K", -1);
    sp.WireH(404.0f, 442.0f, 456.0f);
    sp.Junction({404.0f, 456.0f});
    sp.Junction({442.0f, 456.0f});
    sp.Capacitor({442.0f, 508.0f}, true, "C104", "10u", true, -1, 1);
    sp.WireH(404.0f, 442.0f, 560.0f);
    sp.Junction({442.0f, 560.0f});

    // R112 (33K) column (x=476) + MR100 / MR101 (1S920) bias reference diodes
    sp.WireV(476.0f, yRail67V, yRailChs);
    sp.Junction({476.0f, yRail67V});
    sp.Junction({476.0f, yRailChs});
    sp.Resistor({476.0f, 185.0f}, true, "R112", "33K", -1);
    sp.Diode({476.0f, 628.0f}, 90.0f, "MR100", "1S920", false, -1);
    sp.Diode({476.0f, 668.0f}, 90.0f, "MR101", "1S920", false, -1);

    // =========================================================================
    // DRIVER & THERMAL BIAS STAGE (TR102, TR107, C106, C107, C108, C110, RV101)
    // =========================================================================
    // TR101 collector (x=380, y=577) goes right at y=584 to TR102 base (x=513, y=584)
    const Vec2 tr102Pos{528.0f, 584.0f};
    sp.WireV(380.0f, 577.0f, 584.0f);
    sp.WireH(380.0f, tr102Pos.x - 15.0f, 584.0f);
    sp.Junction({476.0f, 584.0f});

    // C108 (100p) coupling between R112 node (x=476, y=534) and TR102 base/collector
    sp.WireH(476.0f, 518.0f, 534.0f);
    sp.Junction({476.0f, 534.0f});
    sp.Capacitor({497.0f, 534.0f}, false, "C108", "100p", false, -1, 1);
    sp.WireV(518.0f, 534.0f, 584.0f);
    sp.Junction({518.0f, 584.0f});

    // TR102 (U17219) NPN Pre-Driver at (528, 584)
    sp.TransistorBJT(tr102Pos, false, "TR102", "U17219", true, false, 1);
    sp.WireV(528.0f, 148.0f, tr102Pos.y - 15.0f);
    sp.WireV(528.0f, tr102Pos.y + 15.0f, yRailChs);
    sp.Junction({528.0f, yRailChs});
    sp.Resistor({528.0f, 345.0f}, true, "R113", "6.8K", -1);
    sp.Capacitor({528.0f, 480.0f}, true, "C107", "47p", false, -1, 1);
    sp.Resistor({528.0f, 650.0f}, true, "R114", "150", 1);

    // Bootstrap capacitor C106 (47µ) & R116/R117 column (x=592)
    sp.Capacitor({528.0f, 204.0f}, true, "C106", "47u", true, -1, -1);
    sp.WireH(528.0f, 592.0f, 148.0f);
    sp.Junction({592.0f, 148.0f});

    // Bootstrap link from below C106 (x=528, y=292) right to x=708 -> down to Pin 4 line (y=314)
    sp.WireH(528.0f, 708.0f, 292.0f);
    sp.Junction({528.0f, 292.0f});
    sp.WireV(708.0f, 292.0f, 314.0f);
    sp.Junction({708.0f, 314.0f});

    // Bias & Thermal Compensator column (x=592): R116 -> R117 -> TR107 -> TR104 base
    sp.WireV(592.0f, yRail67V, 371.0f);
    sp.Junction({592.0f, yRail67V});
    sp.Resistor({592.0f, 96.0f},  true, "R116", "1.8K", 1);
    sp.Resistor({592.0f, 196.0f}, true, "R117", "1.8K", 1);

    // TR107 (BC109) Active Bias / Thermal Compensator network (center of driver board)
    const Vec2 tr107Pos{592.0f, 386.0f};
    sp.TransistorBJT(tr107Pos, false, "TR107", "BC 109", false, false, 1);
    sp.WireV(592.0f, tr107Pos.y + 15.0f, 534.0f);
    sp.Junction({592.0f, 534.0f});

    // C110 (1000p) across TR107 collector-emitter on left (x=558, y=344..434) with label below plates
    sp.WireV(558.0f, 344.0f, 434.0f);
    sp.WireH(558.0f, 672.0f, 344.0f);
    sp.WireH(558.0f, 672.0f, 434.0f);
    sp.Junction({558.0f, 386.0f});
    sp.Junction({592.0f, 344.0f});
    sp.Junction({592.0f, 434.0f});
    sp.Capacitor({558.0f, 368.0f}, true, "C110", "1000p", false, -1, 2);

    // R131 (6.8K), R132 (1.8K), and RV101 (2.2K) connected bridge on right of TR107
    sp.Resistor({630.0f, 344.0f}, false, "R131", "6.8K", -1);
    sp.Resistor({630.0f, 434.0f}, false, "R132", "1.8K", 1);
    sp.WireV(672.0f, 344.0f, 434.0f);
    sp.Potentiometer({672.0f, 392.0f}, true, "RV101", "2.2K", 1);
    // Connect TR107 base (right side at x=607, y=386) to top of RV101 (x=672, y=366)
    sp.WireH(tr107Pos.x + 15.0f, 672.0f, 366.0f);
    sp.WireV(tr107Pos.x + 15.0f, 366.0f, 386.0f);
    sp.Junction({672.0f, 366.0f});

    // Protection/anti-saturation diodes MR102 & MR103 (1S920)
    sp.WireH(592.0f, 730.0f, 314.0f);
    sp.Junction({592.0f, 314.0f});
    sp.Junction({730.0f, 314.0f});
    sp.Diode({650.0f, 314.0f}, 180.0f, "MR102", "1S920", false, 0);

    sp.WireH(592.0f, 704.0f, 472.0f);
    sp.Junction({592.0f, 472.0f});
    sp.Diode({626.0f, 472.0f}, 0.0f, "MR103", "1S920", false, 1);

    // =========================================================================
    // QUAD "TRIPLES" COMPLEMENTARY OUTPUT STAGE (TR103..TR106, TR1, TR2)
    // =========================================================================
    // Upper Triple: TR103 (U17219) -> TR105 (38496) -> External TR1 (38494)
    const Vec2 tr103Pos{644.0f, 236.0f};
    const Vec2 tr105Pos{730.0f, 190.0f};
    const Vec2 tr1Pos{912.0f, 220.0f};

    sp.WireV(644.0f, yRail67V, tr103Pos.y - 15.0f);
    sp.Junction({644.0f, yRail67V});
    sp.Resistor({644.0f, 120.0f}, true, "R118", "1K", 1);
    sp.TransistorBJT(tr103Pos, false, "TR103", "U17219", true, false, 1);
    sp.WireH(592.0f, tr103Pos.x - 15.0f, 236.0f);
    sp.Junction({592.0f, 236.0f});

    // TR103 emitter (bottom) -> R120 (100) -> Pin 4 line (y=314)
    sp.WireV(644.0f, tr103Pos.y + 15.0f, 266.0f);
    sp.WireH(644.0f, 696.0f, 266.0f);
    sp.WireV(696.0f, 266.0f, 314.0f);
    sp.Junction({696.0f, 314.0f});
    sp.Resistor({674.0f, 266.0f}, false, "R120", "100", -1);

    // TR105 column (x=730): R122 (10) -> TR105 -> R123 (47) -> R124 (.3) -> Output center (y=404)
    sp.WireV(730.0f, yRail67V, 404.0f);
    sp.Junction({730.0f, yRail67V});
    sp.Resistor({730.0f, 118.0f}, true, "R122", "10", 1);
    sp.TransistorBJT(tr105Pos, true, "TR105", "38496", true, true, 1);
    sp.WireH(644.0f, tr105Pos.x - 15.0f, 190.0f);
    sp.Junction({644.0f, 190.0f});
    sp.Resistor({730.0f, 266.0f}, true, "R123", "47", 1);
    sp.Resistor({730.0f, 360.0f}, true, "R124", ".3", 1);

    // Lower Triple: TR104 (U17229) -> TR106 (38495) -> External TR2 (38494)
    const Vec2 tr104Pos{644.0f, 534.0f};
    const Vec2 tr106Pos{730.0f, 584.0f};
    const Vec2 tr2Pos{912.0f, 618.0f};

    sp.TransistorBJT(tr104Pos, true, "TR104", "U17229", true, true, -1);
    sp.WireH(528.0f, tr104Pos.x - 15.0f, 534.0f);
    sp.Junction({528.0f, 534.0f});

    // TR104 emitter (top) -> R121 (100) -> Pin 6 line (y=498)
    sp.WireV(644.0f, 498.0f, tr104Pos.y - 15.0f);
    sp.WireH(644.0f, 730.0f, 498.0f);
    sp.Junction({704.0f, 498.0f});
    sp.WireV(704.0f, 472.0f, 498.0f);
    sp.Resistor({676.0f, 498.0f}, false, "R121", "100", -1);

    // TR104 collector (bottom) -> R119 (1K) -> yRailGnd
    sp.WireV(644.0f, tr104Pos.y + 15.0f, yRailGnd);
    sp.Junction({644.0f, yRailGnd});
    sp.Resistor({644.0f, 648.0f}, true, "R119", "1K", -1);

    // TR106 column (x=730): R125 (.3) -> R126 (47) -> TR106 -> R127 (10) -> yRailGnd
    sp.WireV(730.0f, 404.0f, yRailGnd);
    sp.Junction({730.0f, yRailGnd});
    sp.Resistor({730.0f, 452.0f}, true, "R125", ".3", 1);
    sp.Resistor({730.0f, 534.0f}, true, "R126", "47", 1);
    sp.TransistorBJT(tr106Pos, false, "TR106", "38495", true, false, -1);
    sp.WireH(644.0f, tr106Pos.x - 15.0f, 584.0f);
    sp.Junction({644.0f, 584.0f});
    sp.Resistor({730.0f, 650.0f}, true, "R127", "10", -1);

    // R129 (68) & C109 (.1µ) Zobel stability network at x=774 (from yOut=404 down to yRailGnd)
    const float yOut = 404.0f;
    sp.WireV(774.0f, yOut, yRailGnd);
    sp.Junction({774.0f, yOut});
    sp.Junction({774.0f, yRailGnd});
    sp.Resistor({774.0f, 550.0f}, true, "R129", "68", 1);
    sp.Capacitor({774.0f, 652.0f}, true, "C109", ".1u", false, -1, 1);

    // R115 (2.2) ground return resistor separating input ground (yRailChs) and power ground (yRailGnd)
    sp.WireH(528.0f, 644.0f, yRailGnd);
    sp.Resistor({580.0f, yRailGnd}, false, "R115", "2.2", -1);

    // =========================================================================
    // DRIVER BOARD RIGHT EDGE PINS (1..9 at x=860) & CHASSIS POWER TRANSISTORS
    // =========================================================================
    const float xPinEdge = driverBox.Right(); // 860.0f

    // Pin 2 (y=156): drops from +67V rail at x=812 inside driver board -> Pin 2 -> TR1 collector
    sp.WireV(812.0f, yRail67V, 156.0f);
    sp.Junction({812.0f, yRail67V});
    sp.WireH(812.0f, tr1Pos.x, 156.0f);

    // Pin 3 (y=220): from TR105 collector (x=730, y=220) -> Pin 3 -> TR1 base
    sp.WireH(730.0f, tr1Pos.x - 15.0f, 220.0f);
    sp.Junction({730.0f, 220.0f});

    // Pin 4 (y=314): from R123/R124 node (x=730, y=314) -> Pin 4 -> TR1 emitter
    sp.WireH(730.0f, tr1Pos.x, 314.0f);

    sp.WireV(tr1Pos.x, 156.0f, tr1Pos.y - 15.0f);
    sp.WireV(tr1Pos.x, tr1Pos.y + 15.0f, 314.0f);
    sp.TransistorBJT(tr1Pos, false, "TR1", "38494", true, false, 1);

    // Pin 5 (y=404): Output center rail -> L100 || R128 (10) -> Pin 5 -> C5 (2000µ) -> SK2 / RL / SK3
    sp.WireH(730.0f, 976.0f, yOut);
    sp.Junction({730.0f, yOut});
    sp.InductorH({816.0f, yOut}, "L100", 38.0f);
    sp.WireH(790.0f, 842.0f, yOut + 36.0f);
    sp.WireV(790.0f, yOut, yOut + 36.0f);
    sp.WireV(842.0f, yOut, yOut + 36.0f);
    sp.Junction({790.0f, yOut});
    sp.Junction({842.0f, yOut});
    sp.Resistor({816.0f, yOut + 36.0f}, false, "R128", "10", -1);

    // Pin 6 (y=498): from R125/R126 node (x=730, y=498) -> Pin 6 -> TR2 collector
    sp.WireH(730.0f, tr2Pos.x, 498.0f);
    sp.Junction({730.0f, 498.0f});

    // Pin 7 (y=618): from TR106 emitter (x=730, y=618) -> Pin 7 -> TR2 base
    sp.WireH(730.0f, tr2Pos.x - 15.0f, 618.0f);
    sp.Junction({730.0f, 618.0f});

    sp.WireV(tr2Pos.x, 498.0f, tr2Pos.y - 15.0f);
    sp.WireV(tr2Pos.x, tr2Pos.y + 15.0f, yRailGnd);
    sp.Junction({tr2Pos.x, yRailGnd});
    sp.TransistorBJT(tr2Pos, false, "TR2", "38494", true, false, -1);

    // Pin 8 / Pin 9 link inside driver board at x=822
    sp.WireV(822.0f, yRailGnd, yRailChs);
    sp.Junction({822.0f, yRailGnd});
    sp.Junction({822.0f, yRailChs});

    // Stamp numbered board edge pins 1..9 along xPinEdge = 860, and 10, 11 along x = 118
    sp.BoardPin({xPinEdge, yRail67V}, "1");
    sp.BoardPin({xPinEdge, 156.0f},   "2");
    sp.BoardPin({xPinEdge, 220.0f},   "3");
    sp.BoardPin({xPinEdge, 314.0f},   "4");
    sp.BoardPin({xPinEdge, yOut},     "5");
    sp.BoardPin({xPinEdge, 498.0f},   "6");
    sp.BoardPin({xPinEdge, 618.0f},   "7");
    sp.BoardPin({xPinEdge, yRailGnd}, "8");
    sp.BoardPin({xPinEdge, yRailChs}, "9");
    sp.BoardPin({driverBox.x, yIn},      "10");
    sp.BoardPin({driverBox.x, yRailChs}, "11");

    // Output coupling capacitor C5 (2000µ, 35V) & Speaker Load RL between SK2 and SK3
    scene.DrawText("35V", {870.0f, yOut - 12.0f}, 5.8f, pal.accent, FontStyle::Bold);
    sp.Capacitor({906.0f, yOut}, false, "C5", "2000u", true, -1, -1);

    // Speaker Load RL (dashed resistor) & curved socket contacts SK2 / SK3
    const float xLoad = 950.0f;
    const float ySk3  = 468.0f;
    sp.WireV(xLoad, yOut, yRailChs);
    sp.Junction({xLoad, yOut});
    sp.Junction({xLoad, ySk3});
    sp.Junction({xLoad, yRailChs});
    sp.Resistor({xLoad, (yOut + ySk3) * 0.5f}, true, "RL", "8.2", -1, true);
    sp.WireH(xLoad, 976.0f, ySk3);
    // Curved socket arcs for SK2 (yOut) and SK3 (ySk3)
    scene.DrawArc({980.0f, yOut - 4.0f}, 5.0f, 30.0f, 120.0f, pal.inkBold, 1.3f, 8);
    scene.DrawArc({980.0f, ySk3 + 4.0f}, 5.0f, 210.0f, 120.0f, pal.inkBold, 1.3f, 8);
    scene.DrawText("SK2", {976.0f, yOut - 13.0f}, 5.8f, pal.inkBold, FontStyle::Bold);
    scene.DrawText("SK3", {976.0f, ySk3 + 6.0f},  5.8f, pal.inkBold, FontStyle::Bold);

    // Off-sheet ground arrow to Righthand Amplifier at bottom of TR2 (x=912)
    sp.WireV(tr2Pos.x, yRailGnd, 765.0f);
    sp.SignalArrowDown({tr2Pos.x, 765.0f}, "TO RIGHTHAND", "AMPLIFIER");

    // =========================================================================
    // CENTER SECTION: POWER SUPPLY REGULATOR BOARD M12088
    // =========================================================================
    const Rect psuBox(1012.0f, 120.0f, 328.0f, 540.0f);
    sp.DashedRect(psuBox, 10.0f, 5.0f, 1.4f);

    scene.DrawText("POWER SUPPLY", {1078.0f, 558.0f}, 6.4f, pal.inkBold, FontStyle::Bold);
    scene.DrawLine({1078.0f, 568.5f}, {1146.0f, 568.5f}, pal.ink, 0.9f);
    scene.DrawText("M 12088", {1088.0f, 574.0f}, 6.4f, pal.inkBold, FontStyle::Bold);
    scene.DrawLine({1088.0f, 584.5f}, {1130.0f, 584.5f}, pal.ink, 0.9f);

    const float yPsuTop = 166.0f; // Pin 12 (+67V regulated bus)
    const float yPsuMid = 468.0f; // Pin 13 (0V regulated return bus to Pin 8)
    const float yPsuBot = 636.0f; // Pin 15 (Unregulated -DC rail from bridge)

    sp.WireH(1042.0f, 1368.0f, yPsuTop);
    sp.WireH(1100.0f, 1438.0f, yPsuMid);
    sp.WireH(1042.0f, 1605.0f, yPsuBot);

    // Connect PSU regulated +67V output (Pin 12) up to +67V rail at x=1368
    sp.WireV(1368.0f, yRail67V, yPsuTop);
    sp.Junction({1368.0f, yRail67V});

    // Connect PSU regulated 0V return (Pin 13 at yPsuMid) around bottom of M12088 back to Pin 8 (yRailGnd)
    sp.WireV(1438.0f, yPsuMid, 686.0f);
    sp.Junction({1398.0f, yPsuMid});
    sp.WireH(972.0f, 1438.0f, 686.0f);
    sp.WireV(972.0f, 686.0f, yRailGnd);
    sp.WireH(924.0f, 972.0f, yRailGnd);

    // R200 (15K) & R201 (10K) divider column (x=1042)
    sp.WireV(1042.0f, yPsuTop, yPsuBot);
    sp.Resistor({1042.0f, 344.0f}, true, "R200", "15K", 1);
    sp.Resistor({1042.0f, 514.0f}, true, "R201", "10K", 1);

    // R202 (1.8K), RV200 (4.7K, "SET 67V"), R203 (2.7K), MR200 (1S920)
    sp.WireV(1100.0f, yPsuTop, yPsuMid);
    sp.Junction({1100.0f, yPsuTop});
    sp.Resistor({1100.0f, 240.0f}, true, "R202", "1.8K", -1);
    sp.Potentiometer({1100.0f, 326.0f}, true, "RV200", "4.7K", 1);
    scene.DrawText("SET 67V", {1104.0f, 204.0f}, 6.0f, pal.accent, FontStyle::Bold);
    sp.Resistor({1100.0f, 422.0f}, true, "R203", "2.7K", -1);
    sp.WireH(1042.0f, 1100.0f, 384.0f);
    sp.Junction({1042.0f, 384.0f});
    sp.Junction({1100.0f, 384.0f});
    sp.Diode({1071.0f, 384.0f}, 0.0f, "MR200", "1S920", false, 1);

    // C200 (330p) & TR200 (U17229) PNP Error Amplifier at (1186, 372)
    sp.WireV(1138.0f, yPsuTop, 372.0f);
    sp.Junction({1138.0f, yPsuTop});
    sp.Capacitor({1138.0f, 276.0f}, true, "C200", "330p", false, -1, 1);

    const Vec2 tr200Pos{1186.0f, 372.0f};
    sp.WireH(1100.0f, tr200Pos.x - 15.0f, 372.0f);
    sp.Junction({1100.0f, 372.0f});
    sp.Junction({1138.0f, 372.0f});
    sp.TransistorBJT(tr200Pos, true, "TR200", "U17229", true, true, -1);

    // TR200 emitter (top) connects to node between R204 (3.3K) and MR201 (LR120C) at y=336
    sp.WireV(1186.0f, 336.0f, tr200Pos.y - 15.0f);
    sp.WireH(1186.0f, 1242.0f, 336.0f);
    sp.Junction({1242.0f, 336.0f});

    // R204 (3.3K) & MR201 (LR120C / 1S2120) Zener reference column (x=1242, from yPsuTop to yPsuMid)
    sp.WireV(1242.0f, yPsuTop, yPsuMid);
    sp.Junction({1242.0f, yPsuTop});
    sp.Junction({1242.0f, yPsuMid});
    sp.Resistor({1242.0f, 246.0f}, true, "R204", "3.3K", -1);
    sp.Diode({1242.0f, 398.0f}, 270.0f, "MR201", "LR120C", true, 1);

    // C201 (0.22µ) filter cap (x=1298, from yPsuTop to yPsuMid)
    sp.WireV(1298.0f, yPsuTop, yPsuMid);
    sp.Junction({1298.0f, yPsuTop});
    sp.Junction({1298.0f, yPsuMid});
    sp.Capacitor({1298.0f, 318.0f}, true, "C201", "0.22u", false, -1, 1);

    // Series Pass Regulator Driver TR201 (38496) & External Chassis Power Transistor TR3 (38119)
    const Vec2 tr201Pos{1256.0f, 540.0f};
    const Vec2 tr3Pos{1398.0f, 566.0f};
    sp.WireV(1186.0f, tr200Pos.y + 15.0f, yPsuBot);
    sp.Junction({1186.0f, yPsuBot});
    sp.Resistor({1186.0f, 594.0f}, true, "R208", "15K", -1);

    sp.WireH(1186.0f, tr201Pos.x - 15.0f, 540.0f);
    sp.Junction({1186.0f, 540.0f});
    sp.TransistorBJT(tr201Pos, true, "TR201", "38496", true, true, -1);

    sp.WireV(1256.0f, yPsuMid, tr201Pos.y - 15.0f);
    sp.Junction({1256.0f, yPsuMid});
    sp.WireV(1256.0f, tr201Pos.y + 15.0f, yPsuBot);
    sp.Junction({1256.0f, yPsuBot});
    sp.Resistor({1256.0f, 496.0f}, true, "R206", "100", 1);
    sp.Resistor({1256.0f, 598.0f}, true, "R207", "68", 1);

    // Pin 13 (y=468), Pin 14 (y=566), Pin 15 (y=636) to External TR3 (38119)
    sp.WireH(1256.0f, tr3Pos.x - 15.0f, 566.0f);
    sp.Junction({1256.0f, 566.0f});
    sp.WireV(tr3Pos.x, yPsuMid, tr3Pos.y - 15.0f);
    sp.WireV(tr3Pos.x, tr3Pos.y + 15.0f, yPsuBot);
    sp.Junction({tr3Pos.x, yPsuBot});
    sp.TransistorBJT(tr3Pos, false, "TR3", "38119", true, false, -1);

    // Stamp PSU Board Edge Pins 12..15 at x=1340
    sp.BoardPin({psuBox.Right(), yPsuTop}, "12");
    sp.BoardPin({psuBox.Right(), yPsuMid}, "13");
    sp.BoardPin({psuBox.Right(), 566.0f},  "14");
    sp.BoardPin({psuBox.Right(), yPsuBot}, "15");

    // =========================================================================
    // RECTIFIER BRIDGE (MR1..MR4), RESERVOIR CAPS (C2, C3, C4) & MAINS T1
    // =========================================================================
    // Reservoir Electrolytic Capacitors C2 (2000µ) & C3 (2000µ) in compact parallel loop
    const float xResCenter = 1502.0f;
    const float yResTop = 242.0f;
    const float yResBot = 316.0f;
    sp.WireV(xResCenter, yRail67V, yResTop);
    sp.WireV(xResCenter, yResBot, yPsuBot);
    sp.Junction({xResCenter, yRail67V});
    sp.Junction({xResCenter, yPsuBot});
    sp.Junction({xResCenter, yResTop});
    sp.Junction({xResCenter, yResBot});
    sp.WireH(1478.0f, 1526.0f, yResTop);
    sp.WireH(1478.0f, 1526.0f, yResBot);
    sp.WireV(1478.0f, yResTop, yResBot);
    sp.WireV(1526.0f, yResTop, yResBot);
    sp.Capacitor({1478.0f, 279.0f}, true, "C2", "2000u", true, -1, -1);
    sp.Capacitor({1526.0f, 279.0f}, true, "C3", "2000u", true, -1, 1);

    // Full-wave Diode Bridge at (1605, 344)
    const Vec2 bridgeCenter{1605.0f, 344.0f};
    const float bridgeR = 44.0f;
    sp.DiodeBridge(bridgeCenter, bridgeR);

    // Connect Bridge Top (+DC) to yRail67V and Bottom (-DC) to yPsuBot
    sp.WireV(bridgeCenter.x, yRail67V, bridgeCenter.y - bridgeR);
    sp.WireV(bridgeCenter.x, bridgeCenter.y + bridgeR, yPsuBot);

    // AC input to bridge from Transformer T1 secondary (top at y=344, bottom at y=444)
    const float xT1Sec = 1712.0f;
    const float ySecTop = 344.0f;
    const float ySecBot = 444.0f;
    // Right AC corner of bridge -> straight right to top of T1 secondary
    sp.WireH(bridgeCenter.x + bridgeR, xT1Sec, ySecTop);
    // Left AC corner of bridge -> left to x=1544, down to y=444, right under bridge to bottom of T1 secondary
    sp.WireH(1544.0f, bridgeCenter.x - bridgeR, bridgeCenter.y);
    sp.WireV(1544.0f, bridgeCenter.y, ySecBot);
    sp.WireH(1544.0f, xT1Sec, ySecBot);

    // C4 (.01µ) RF suppression capacitor across secondary at x=1676
    sp.WireV(1676.0f, ySecTop, ySecBot);
    sp.Junction({1676.0f, ySecTop});
    sp.Junction({1676.0f, ySecBot});
    sp.Capacitor({1676.0f, 408.0f}, true, "C4", ".01u", false, -1, -1);

    // Mains Transformer T1, Selector, R2, V1, FS1, PL1
    sp.MainsTransformerSection({xT1Sec, 224.0f}, ySecTop, ySecBot);

    // =========================================================================
    // RIGHT SECTION: COPPER-SIDE PCB OUTLINES & HISTORICAL SERVICE NOTES
    // =========================================================================
    sp.PcbCopperOutlines(
        Rect(1884.0f, 62.0f, 276.0f, 232.0f),
        Rect(1884.0f, 396.0f, 276.0f, 148.0f)
    );

    // Bottom Left Note: ALTERNATIVE TRANSISTOR TYPES
    const float noteX1 = 280.0f;
    const float noteY1 = 742.0f;
    scene.DrawText("ALTERNATIVE  TRANSISTOR  TYPES.", {noteX1, noteY1}, 6.6f, pal.inkBold, FontStyle::Bold);
    scene.DrawLine({noteX1, noteY1 + 10.5f}, {noteX1 + 188.0f, noteY1 + 10.5f}, pal.ink, 0.9f);
    scene.DrawText("BC 154   =  BCY 70.",            {noteX1 + 26.0f, noteY1 + 17.0f}, 6.2f, pal.inkBold, FontStyle::Bold);
    scene.DrawText("U 17219  =  2TX 304,  MPS A06",  {noteX1 + 26.0f, noteY1 + 29.0f}, 6.2f, pal.inkBold, FontStyle::Bold);
    scene.DrawText("            2S 497,   BS 688.",  {noteX1 + 26.0f, noteY1 + 40.0f}, 6.2f, pal.inkBold, FontStyle::Bold);
    scene.DrawText("U 17229  =  2TX 504,  MPS A56",  {noteX1 + 26.0f, noteY1 + 52.0f}, 6.2f, pal.inkBold, FontStyle::Bold);
    scene.DrawText("            2S 498.",            {noteX1 + 26.0f, noteY1 + 63.0f}, 6.2f, pal.inkBold, FontStyle::Bold);

    // Bottom Center Notes
    const float noteX2 = 1042.0f;
    const float noteY2 = 738.0f;
    scene.DrawText("THE  RIGHTHAND  AMPLIFIER", {noteX2, noteY2},        6.2f, pal.inkBold, FontStyle::Bold);
    scene.DrawText("IS  IDENTICAL  TO",         {noteX2 + 12.0f, noteY2 + 11.0f}, 6.2f, pal.inkBold, FontStyle::Bold);
    scene.DrawText("THE  LEFTHAND  AMPLIFIER",  {noteX2, noteY2 + 22.0f}, 6.2f, pal.inkBold, FontStyle::Bold);

    const float noteX3 = 1492.0f;
    const float noteY3 = 702.0f;
    scene.DrawText("THE  FRONT  BOARD  AND  THE",  {noteX3, noteY3},        6.0f, pal.inkBold, FontStyle::Bold);
    scene.DrawText("BOTTOM  TWO  TRANSISTORS",     {noteX3, noteY3 + 11.0f}, 6.0f, pal.inkBold, FontStyle::Bold);
    scene.DrawText("ON  THE  HEAT  SINK  ARE  THE",{noteX3, noteY3 + 22.0f}, 6.0f, pal.inkBold, FontStyle::Bold);
    scene.DrawText("LEFTHAND  CHANNEL.",           {noteX3 + 18.0f, noteY3 + 33.0f}, 6.0f, pal.inkBold, FontStyle::Bold);

    // =========================================================================
    // BOTTOM-RIGHT ENGINEERING TITLE BLOCK (M12160)
    // =========================================================================
    sp.QuadTitleBlock(Rect(1236.0f, 774.0f, 940.0f, 118.0f));
}

} // namespace guikit::demo
