#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>
#include "../DemoContext.hpp"

namespace guikit::demo {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kDegToRad = kPi / 180.0f;

struct AttitudeDemoState {
    bool initialized{false};
    int controlModeIndex{0};        // 0 = Mouse Follow (Direct), 1 = Virtual Stick (FBW 360 Roll), 2 = Autopilot Figure-8, 3 = Manual Sliders
    int displayModeIndex{0};        // 0 = FullPFD, 1 = CompactADI, 2 = HudOverlay
    int referenceModeIndex{0};      // 0 = WesternMovingHorizon, 1 = EasternMovingAircraft (KPPI)
    int colorSchemeIndex{0};        // 0 = GlassCockpit, 1 = HudGreen, 2 = TacticalDark
    int fdStyleIndex{1};            // 0 = Off, 1 = SingleCueVBar, 2 = DualCueCrossbars
    int aircraftSymbolIndex{0};     // 0 = GullWing, 1 = ClassicW, 2 = RussianSilhouette

    // Primary Flight Telemetry
    float rollDeg{0.0f};
    float pitchDeg{2.5f};
    float headingDeg{42.0f};
    float airspeedKnots{250.0f};
    float machNumber{0.72f};
    float altitudeFeet{12500.0f};
    float verticalSpeedFpm{0.0f};
    float slipSkid{0.0f};
    float gLoad{1.0f};
    float radioAltitudeFeet{1850.0f};

    // FPV, FD & Bugs
    bool showFlightPathVector{true};
    float aoaDeg{2.2f};
    float sideslipDeg{0.0f};
    float fdPitchDeg{3.0f};
    float fdRollDeg{0.0f};
    float targetAirspeedBug{250.0f};
    float targetAltitudeBug{12500.0f};
    float targetHeadingBug{45.0f};
    float baroSettingInHg{29.92f};
    bool baroStd{false};

    // ILS Deviation
    bool showIlsDiamonds{true};
    float locDeviationDots{0.0f};
    float gsDeviationDots{0.0f};

    // Instrument Overlays
    bool showPitchLadder{true};
    bool showBankScale{true};
    bool showSpeedAndAltTapes{true};
    bool showHeadingTape{true};
    bool showFmaBar{true};
    bool showMouseStickOverlay{true};
    float fovPitchDeg{50.0f};

    double simTimeSec{0.0};
    std::chrono::steady_clock::time_point lastWallClock{};
    AttitudeViewResponse lastResp{};
};

AttitudeDemoState s_att{};

[[nodiscard]] inline float WrapHeadingDeg(float d) noexcept {
    float r = std::fmod(d, 360.0f);
    if (r < 0.0f) r += 360.0f;
    return r;
}

[[nodiscard]] inline float WrapRollDeg(float d) noexcept {
    float r = std::fmod(d + 180.0f, 360.0f);
    if (r < 0.0f) r += 360.0f;
    return r - 180.0f;
}

void StepAttitudeSimulation(guikit::Context& ui, AttitudeDemoState& st) {
    const auto now = std::chrono::steady_clock::now();
    if (!st.initialized) {
        st.lastWallClock = now;
        st.initialized = true;
    }

    double dt = std::chrono::duration<double>(now - st.lastWallClock).count();
    st.lastWallClock = now;
    dt = std::clamp(dt, 0.001, 0.05);
    const float fdt = static_cast<float>(dt);
    st.simTimeSec += dt;

    if (st.controlModeIndex == 3) {
        // Manual mode: keep derived Mach and G-load coherent with sliders
        st.machNumber = std::clamp(st.airspeedKnots / 348.0f, 0.10f, 0.99f);
        return;
    }

    const Vec2 mousePos = ui.GetMousePos();
    const Rect adiR = st.lastResp.adiRect;
    const Vec2 adiCenter = (adiR.w > 20.0f) ? adiR.Center() : Vec2{640.0f, 400.0f};
    const float adiRadius = (adiR.w > 20.0f) ? (adiR.w * 0.5f) : 220.0f;

    const float nx = std::clamp((mousePos.x - adiCenter.x) / std::max(40.0f, adiRadius), -1.0f, 1.0f);
    const float ny = std::clamp((mousePos.y - adiCenter.y) / std::max(40.0f, adiRadius), -1.0f, 1.0f);
    const bool mouseInStage = (adiR.w > 20.0f) &&
                              mousePos.x >= (adiR.x - 160.0f) && mousePos.x <= (adiR.Right() + 160.0f) &&
                              mousePos.y >= (adiR.y - 80.0f)  && mousePos.y <= (adiR.Bottom() + 80.0f);

    const float prevRoll = st.rollDeg;
    const float prevPitch = st.pitchDeg;

    if (st.controlModeIndex == 0) {
        // Mode 0: Direct Mouse Follow (Mouse position maps directly to Roll & Pitch)
        const float cmdRoll = mouseInStage ? (nx * 65.0f) : 0.0f;
        const float cmdPitch = mouseInStage ? (-ny * 38.0f) : 2.5f;
        const float alpha = std::clamp(fdt * 12.0f, 0.05f, 1.0f);
        st.rollDeg += (cmdRoll - st.rollDeg) * alpha;
        st.pitchDeg += (cmdPitch - st.pitchDeg) * alpha;
        st.fdRollDeg = cmdRoll;
        st.fdPitchDeg = cmdPitch;
    } else if (st.controlModeIndex == 1) {
        // Mode 1: Virtual Fly-By-Wire Side-Stick (Mouse offset commands Roll Rate & Pitch Rate, supports 360 barrel rolls)
        if (mouseInStage) {
            const float deadzone = 0.05f;
            const float sx = (std::abs(nx) > deadzone) ? ((nx - std::copysign(deadzone, nx)) / (1.0f - deadzone)) : 0.0f;
            const float sy = (std::abs(ny) > deadzone) ? ((ny - std::copysign(deadzone, ny)) / (1.0f - deadzone)) : 0.0f;
            const float rollRateDegPerSec = sx * 135.0f;
            const float pitchRateDegPerSec = -sy * 65.0f;
            st.rollDeg = WrapRollDeg(st.rollDeg + rollRateDegPerSec * fdt);
            st.pitchDeg = std::clamp(st.pitchDeg + pitchRateDegPerSec * fdt, -85.0f, 85.0f);
            st.fdRollDeg = std::clamp(sx * 45.0f, -60.0f, 60.0f);
            st.fdPitchDeg = std::clamp(st.pitchDeg + pitchRateDegPerSec * 0.25f, -30.0f, 30.0f);
        }
    } else if (st.controlModeIndex == 2) {
        // Mode 2: Automated Autopilot Figure-8 Flight Pattern
        const float t = static_cast<float>(st.simTimeSec);
        const float targetRoll = 36.0f * std::sin(t * 0.55f);
        const float targetPitch = 3.0f + 9.5f * std::sin(t * 1.10f);
        st.fdRollDeg = 36.0f * std::sin((t + 0.45f) * 0.55f);
        st.fdPitchDeg = 3.0f + 9.5f * std::sin((t + 0.45f) * 1.10f);
        const float alpha = std::clamp(fdt * 6.0f, 0.02f, 1.0f);
        st.rollDeg += (targetRoll - st.rollDeg) * alpha;
        st.pitchDeg += (targetPitch - st.pitchDeg) * alpha;
    }

    // Integrate realistic coupled air-data dynamics from Roll & Pitch
    const float rollRate = (st.rollDeg - prevRoll) / fdt;
    const float pitchRate = (st.pitchDeg - prevPitch) / fdt;

    // Vertical speed (FPM) driven by pitch angle and airspeed
    const float targetVsi = std::sin((st.pitchDeg - 2.0f) * kDegToRad) * st.airspeedKnots * 88.0f;
    st.verticalSpeedFpm += (targetVsi - st.verticalSpeedFpm) * std::clamp(fdt * 6.0f, 0.05f, 1.0f);
    st.altitudeFeet = std::clamp(st.altitudeFeet + (st.verticalSpeedFpm / 60.0f) * fdt, 0.0f, 50000.0f);
    st.radioAltitudeFeet = std::clamp(st.altitudeFeet - 10650.0f, 20.0f, 2500.0f);

    // Airspeed responds to dive/climb + autothrottle restoring toward targetAirspeedBug
    const float gravityAccelKtPerSec = -std::sin((st.pitchDeg - 2.0f) * kDegToRad) * 18.0f;
    const float autoThrottleKtPerSec = (st.targetAirspeedBug - st.airspeedKnots) * 0.45f;
    st.airspeedKnots = std::clamp(st.airspeedKnots + (gravityAccelKtPerSec + autoThrottleKtPerSec) * fdt, 95.0f, 380.0f);
    st.machNumber = std::clamp(st.airspeedKnots / 348.0f, 0.15f, 0.99f);

    // Coordinated turn rate integrates magnetic heading
    const float turnRateDegPerSec = std::clamp(std::tan( std::clamp(st.rollDeg, -70.0f, 70.0f) * kDegToRad ) * 4.2f, -18.0f, 18.0f);
    st.headingDeg = WrapHeadingDeg(st.headingDeg + turnRateDegPerSec * fdt);

    // Normal load factor (G) and inclinometer slip/skid ball
    const float bankCos = std::max(0.35f, std::cos(std::clamp(st.rollDeg, -70.0f, 70.0f) * kDegToRad));
    const float targetG = (1.0f / bankCos) + pitchRate * 0.035f;
    st.gLoad += (std::clamp(targetG, -2.5f, 6.0f) - st.gLoad) * std::clamp(fdt * 8.0f, 0.05f, 1.0f);

    const float targetSlip = std::clamp(-rollRate * 0.008f, -0.85f, 0.85f);
    st.slipSkid += (targetSlip - st.slipSkid) * std::clamp(fdt * 7.0f, 0.05f, 1.0f);
    st.aoaDeg = std::clamp(2.0f + (st.gLoad - 1.0f) * 2.4f, -4.0f, 14.0f);
    st.sideslipDeg = st.slipSkid * 3.5f;

    // Smoothly animate ILS Localizer & Glideslope deviation relative to target bugs
    st.locDeviationDots = std::clamp(std::sin(static_cast<float>(st.simTimeSec) * 0.35f) * 0.8f - st.rollDeg * 0.02f, -2.2f, 2.2f);
    st.gsDeviationDots = std::clamp((st.targetAltitudeBug - st.altitudeFeet) / 250.0f, -2.2f, 2.2f);
}

} // namespace

void RenderAttitudeInspector(guikit::Context& ui, DemoContext& /*ctx*/) {
    static constexpr std::array<std::string_view, 4> kControlModes = {
        "Mouse Follow (Direct)",
        "Virtual Stick (FBW 360°)",
        "Autopilot Figure-8",
        "Manual Sliders"
    };
    static constexpr std::array<std::string_view, 3> kDisplayModes = {
        "Full PFD (Primary Flight Display)",
        "Compact ADI (Circular Dial)",
        "HUD Overlay (Collimator)"
    };
    static constexpr std::array<std::string_view, 2> kRefModes = {
        "Western (Moving Horizon)",
        "Russian KPPI (Moving Aircraft)"
    };
    static constexpr std::array<std::string_view, 3> kColorSchemes = {
        "Glass Cockpit (Sky/Earth)",
        "HUD Phosphor Green",
        "Tactical Dark"
    };
    static constexpr std::array<std::string_view, 3> kFdStyles = {
        "Off",
        "Single-Cue V-Bar",
        "Dual-Cue Crossbars"
    };
    static constexpr std::array<std::string_view, 3> kAircraftSymbols = {
        "Gull-Wing (Garmin / Airbus)",
        "Classic W-Wing",
        "Russian KPPI Silhouette"
    };

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Flight Control Mode",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Select(s_att.controlModeIndex, kControlModes);
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Display Mode",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Select(s_att.displayModeIndex, kDisplayModes);
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Horizon Reference Frame",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Select(s_att.referenceModeIndex, kRefModes);
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Color Scheme",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Select(s_att.colorSchemeIndex, kColorSchemes);
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Flight Director (FD)",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Select(s_att.fdStyleIndex, kFdStyles);
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Aircraft Reference Symbol",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Select(s_att.aircraftSymbolIndex, kAircraftSymbols);
    }

    ui.Size(guikit::UiSize::Xs).Heading({.label = "Attitude & Air Data", .divider = true});

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Roll / Bank Angle",
        .layout = guikit::ElementLayout::Top
    })) {
        if (ui.Size(guikit::UiSize::Sm).Slider(s_att.rollDeg, -180.0f, 180.0f, {
            .valueFormat = "%+.1f deg"
        }).changed) {
            s_att.controlModeIndex = 3;
        }
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Pitch Angle",
        .layout = guikit::ElementLayout::Top
    })) {
        if (ui.Size(guikit::UiSize::Sm).Slider(s_att.pitchDeg, -85.0f, 85.0f, {
            .valueFormat = "%+.1f deg"
        }).changed) {
            s_att.controlModeIndex = 3;
        }
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Magnetic Heading",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Slider(s_att.headingDeg, 0.0f, 359.0f, {
            .valueFormat = "%03.0f deg"
        });
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Airspeed (IAS)",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Slider(s_att.airspeedKnots, 80.0f, 380.0f, {
            .valueFormat = "%.0f kt"
        });
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Altitude",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Slider(s_att.altitudeFeet, 0.0f, 42000.0f, {
            .valueFormat = "%.0f ft"
        });
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Pitch Field of View",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Slider(s_att.fovPitchDeg, 30.0f, 80.0f, {
            .valueFormat = "%.0f deg"
        });
    }

    ui.Size(guikit::UiSize::Xs).Heading({.label = "Symbology & Tapes", .divider = true});

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "Pitch Ladder & Chevrons"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_att.showPitchLadder);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "Bank Arc & Slip/Skid Ball"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_att.showBankScale);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "Flight Path Vector (FPV)"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_att.showFlightPathVector);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "ILS Localizer & Glideslope"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_att.showIlsDiamonds);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "Speed & Altitude Tapes"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_att.showSpeedAndAltTapes);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "Compass Heading Tape"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_att.showHeadingTape);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "Flight Mode Annunciator (FMA)"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_att.showFmaBar);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "Mouse Stick Vector Overlay"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_att.showMouseStickOverlay);
    }
}

void RenderAttitudeStage(guikit::Context& ui, DemoContext& /*ctx*/) {
    StepAttitudeSimulation(ui, s_att);
    if (s_att.controlModeIndex != 3) {
        ui.RequestRepaint(2);
    }

    // Top Avionics Control Strip
    if (auto tb = ui.Size(guikit::UiSize::Sm).Toolbar()) {
        if (tb.ToolButton({
            .text = "Mouse Follow",
            .icon = guikit::Icon::Play(),
            .color = (s_att.controlModeIndex == 0) ? guikit::StateColor::Success() : guikit::StateColor::Ghost(),
            .active = (s_att.controlModeIndex == 0)
        })) {
            s_att.controlModeIndex = 0;
        }
        if (tb.ToolButton({
            .text = "Virtual Stick (360°)",
            .color = (s_att.controlModeIndex == 1) ? guikit::StateColor::Success() : guikit::StateColor::Ghost(),
            .active = (s_att.controlModeIndex == 1)
        })) {
            s_att.controlModeIndex = 1;
        }
        if (tb.ToolButton({
            .text = "Autopilot Fig-8",
            .color = (s_att.controlModeIndex == 2) ? guikit::StateColor::Info() : guikit::StateColor::Ghost(),
            .active = (s_att.controlModeIndex == 2)
        })) {
            s_att.controlModeIndex = 2;
        }
        if (tb.ToolButton({
            .text = "Manual",
            .color = (s_att.controlModeIndex == 3) ? guikit::StateColor::Warning() : guikit::StateColor::Ghost(),
            .active = (s_att.controlModeIndex == 3)
        })) {
            s_att.controlModeIndex = 3;
        }

        tb.Separator();

        if (tb.ToolButton({
            .text = "Full PFD",
            .color = (s_att.displayModeIndex == 0) ? guikit::StateColor::Primary() : guikit::StateColor::Ghost(),
            .active = (s_att.displayModeIndex == 0)
        })) {
            s_att.displayModeIndex = 0;
        }
        if (tb.ToolButton({
            .text = "Compact ADI",
            .color = (s_att.displayModeIndex == 1) ? guikit::StateColor::Primary() : guikit::StateColor::Ghost(),
            .active = (s_att.displayModeIndex == 1)
        })) {
            s_att.displayModeIndex = 1;
        }
        if (tb.ToolButton({
            .text = "HUD Overlay",
            .color = (s_att.displayModeIndex == 2) ? guikit::StateColor::Primary() : guikit::StateColor::Ghost(),
            .active = (s_att.displayModeIndex == 2)
        })) {
            s_att.displayModeIndex = 2;
        }

        tb.Separator();

        if (tb.ToolButton({
            .text = (s_att.referenceModeIndex == 0) ? "Ref: Western" : "Ref: KPPI",
            .color = (s_att.referenceModeIndex == 1) ? guikit::StateColor::Info() : guikit::StateColor::Ghost(),
            .active = (s_att.referenceModeIndex == 1)
        })) {
            s_att.referenceModeIndex = (s_att.referenceModeIndex + 1) % 2;
            if (s_att.referenceModeIndex == 1) {
                s_att.aircraftSymbolIndex = 2; // Russian KPPI silhouette
            } else if (s_att.aircraftSymbolIndex == 2) {
                s_att.aircraftSymbolIndex = 0;
            }
        }

        tb.Spacer();

        if (tb.ToolButton({
            .text = "Level Wings",
            .icon = guikit::Icon::Refresh()
        })) {
            s_att.rollDeg = 0.0f;
            s_att.pitchDeg = 2.0f;
            s_att.verticalSpeedFpm = 0.0f;
            s_att.slipSkid = 0.0f;
        }
    }

    // Main Autonomous AttitudeView Widget
    s_att.lastResp = ui.AttitudeView({
        .displayMode = static_cast<guikit::AttitudeDisplayMode>(s_att.displayModeIndex),
        .referenceMode = static_cast<guikit::HorizonReferenceMode>(s_att.referenceModeIndex),
        .colorScheme = static_cast<guikit::AttitudeColorScheme>(s_att.colorSchemeIndex),
        .flightDirectorStyle = static_cast<guikit::FlightDirectorStyle>(s_att.fdStyleIndex),
        .aircraftSymbol = static_cast<guikit::AircraftSymbolStyle>(s_att.aircraftSymbolIndex),
        .rollDeg = s_att.rollDeg,
        .pitchDeg = s_att.pitchDeg,
        .headingDeg = s_att.headingDeg,
        .airspeedKnots = s_att.airspeedKnots,
        .machNumber = s_att.machNumber,
        .altitudeFeet = s_att.altitudeFeet,
        .verticalSpeedFpm = s_att.verticalSpeedFpm,
        .slipSkid = s_att.slipSkid,
        .gLoad = s_att.gLoad,
        .radioAltitudeFeet = s_att.radioAltitudeFeet,
        .showFlightPathVector = s_att.showFlightPathVector,
        .aoaDeg = s_att.aoaDeg,
        .sideslipDeg = s_att.sideslipDeg,
        .fdPitchDeg = s_att.fdPitchDeg,
        .fdRollDeg = s_att.fdRollDeg,
        .targetAirspeedBug = s_att.targetAirspeedBug,
        .targetAltitudeBug = s_att.targetAltitudeBug,
        .targetHeadingBug = s_att.targetHeadingBug,
        .baroSettingInHg = s_att.baroSettingInHg,
        .baroStd = s_att.baroStd,
        .showIlsDiamonds = s_att.showIlsDiamonds,
        .locDeviationDots = s_att.locDeviationDots,
        .gsDeviationDots = s_att.gsDeviationDots,
        .showPitchLadder = s_att.showPitchLadder,
        .showBankScale = s_att.showBankScale,
        .showSpeedAndAltTapes = s_att.showSpeedAndAltTapes,
        .showHeadingTape = s_att.showHeadingTape,
        .showFmaBar = s_att.showFmaBar,
        .showMouseStickOverlay = s_att.showMouseStickOverlay && (s_att.controlModeIndex <= 1),
        .fieldOfViewPitchDeg = s_att.fovPitchDeg,
        .fovPitchDegPtr = &s_att.fovPitchDeg,
        .heightMode = guikit::SizeMode::Stretch
    });
}

std::string GenerateAttitudeCode(const DemoContext& /*ctx*/) {
    static constexpr std::array<const char*, 3> kModeEnums = {
        "AttitudeDisplayMode::FullPFD",
        "AttitudeDisplayMode::CompactADI",
        "AttitudeDisplayMode::HudOverlay"
    };
    static constexpr std::array<const char*, 2> kRefEnums = {
        "HorizonReferenceMode::WesternMovingHorizon",
        "HorizonReferenceMode::EasternMovingAircraft"
    };
    static constexpr std::array<const char*, 3> kSchemeEnums = {
        "AttitudeColorScheme::GlassCockpit",
        "AttitudeColorScheme::HudGreen",
        "AttitudeColorScheme::TacticalDark"
    };
    static constexpr std::array<const char*, 3> kFdEnums = {
        "FlightDirectorStyle::Off",
        "FlightDirectorStyle::SingleCueVBar",
        "FlightDirectorStyle::DualCueCrossbars"
    };

    char buf[2048];
    std::snprintf(
        buf,
        sizeof(buf),
        "// Avionics Primary Flight Display (PFD) & Artificial Horizon Widget\n"
        "auto resp = ui.AttitudeView({\n"
        "    .displayMode          = guikit::%s,\n"
        "    .referenceMode        = guikit::%s,\n"
        "    .colorScheme          = guikit::%s,\n"
        "    .flightDirectorStyle  = guikit::%s,\n"
        "    .rollDeg              = %.1ff,\n"
        "    .pitchDeg             = %.1ff,\n"
        "    .headingDeg           = %.1ff,\n"
        "    .airspeedKnots        = %.1ff,\n"
        "    .machNumber           = %.2ff,\n"
        "    .altitudeFeet         = %.0ff,\n"
        "    .verticalSpeedFpm     = %.0ff,\n"
        "    .slipSkid             = %.2ff,\n"
        "    .gLoad                = %.2ff,\n"
        "    .showFlightPathVector = %s,\n"
        "    .showIlsDiamonds      = %s,\n"
        "    .fovPitchDegPtr       = &fovPitchDeg,\n"
        "    .heightMode           = guikit::SizeMode::Stretch\n"
        "});\n"
        "if (resp.cursorInsideAdi) {\n"
        "    // Mouse stick telemetry: roll = %.1f deg, pitch = %.1f deg\n"
        "}\n",
        kModeEnums[std::clamp(s_att.displayModeIndex, 0, 2)],
        kRefEnums[std::clamp(s_att.referenceModeIndex, 0, 1)],
        kSchemeEnums[std::clamp(s_att.colorSchemeIndex, 0, 2)],
        kFdEnums[std::clamp(s_att.fdStyleIndex, 0, 2)],
        s_att.rollDeg,
        s_att.pitchDeg,
        s_att.headingDeg,
        s_att.airspeedKnots,
        s_att.machNumber,
        s_att.altitudeFeet,
        s_att.verticalSpeedFpm,
        s_att.slipSkid,
        s_att.gLoad,
        s_att.showFlightPathVector ? "true" : "false",
        s_att.showIlsDiamonds ? "true" : "false",
        s_att.lastResp.cursorRollDeg,
        s_att.lastResp.cursorPitchDeg
    );
    return std::string(buf);
}

} // namespace guikit::demo
