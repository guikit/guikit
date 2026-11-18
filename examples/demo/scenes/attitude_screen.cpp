#include <cmath>
#include "../demo_context.hpp"

namespace guikit::demo {

namespace {

struct AttitudeDemoState {
    bool initialized{false};
    keel::i32 controlModeIndex{0};        // 0 = Mouse Follow (Direct), 1 = Virtual Stick (FBW 360 Roll), 2 = Autopilot Figure-8, 3 = Manual Sliders
    keel::i32 displayModeIndex{0};        // 0 = FullPFD, 1 = CompactADI, 2 = HudOverlay
    keel::i32 viewportModeIndex{1};       // 0 = Auto, 1 = FullBleed, 2 = RoundedDome, 3 = Circular
    keel::i32 referenceModeIndex{0};      // 0 = WesternMovingHorizon, 1 = EasternMovingAircraft (KPPI)
    keel::i32 colorSchemeIndex{0};        // 0 = GlassCockpit, 1 = HudGreen, 2 = TacticalDark
    keel::i32 fdStyleIndex{1};            // 0 = Off, 1 = SingleCueVBar, 2 = DualCueCrossbars
    keel::i32 aircraftSymbolIndex{0};     // 0 = GullWing, 1 = ClassicW, 2 = RussianSilhouette

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
    keel::u64 lastWallNs{0};
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
    const keel::u64 nowNs = keel::monotonic_ns();
    if (!st.initialized) {
        st.lastWallNs = nowNs;
        st.initialized = true;
    }

    double dt = static_cast<double>(nowNs - st.lastWallNs) * 1e-9;
    st.lastWallNs = nowNs;
    dt = keel::clamp(dt, 0.001, 0.05);
    const float fdt = static_cast<float>(dt);
    st.simTimeSec += dt;

    if (st.controlModeIndex == 3) {
        // Manual mode: keep derived Mach and G-load coherent with sliders
        st.machNumber = keel::clamp(st.airspeedKnots / 348.0f, 0.10f, 0.99f);
        return;
    }

    const Vec2 mousePos = ui.GetMousePos();
    const Rect adiR = st.lastResp.adiRect;
    const Vec2 adiCenter = (adiR.w > 20.0f) ? adiR.Center() : Vec2{640.0f, 400.0f};
    const float adiRadius = (adiR.w > 20.0f) ? (adiR.w * 0.5f) : 220.0f;

    const float nx = keel::clamp((mousePos.x - adiCenter.x) / keel::max(40.0f, adiRadius), -1.0f, 1.0f);
    const float ny = keel::clamp((mousePos.y - adiCenter.y) / keel::max(40.0f, adiRadius), -1.0f, 1.0f);
    const Rect horR = (st.lastResp.horizonRect.w > 20.0f) ? st.lastResp.horizonRect : adiR;
    const bool mouseInStage = (horR.w > 20.0f) &&
                              mousePos.x >= (horR.x - 160.0f) && mousePos.x <= (horR.Right() + 160.0f) &&
                              mousePos.y >= (horR.y - 80.0f)  && mousePos.y <= (horR.Bottom() + 80.0f);

    const float prevRoll = st.rollDeg;
    const float prevPitch = st.pitchDeg;

    if (st.controlModeIndex == 0) {
        // Mode 0: Direct Mouse Follow (Mouse position maps directly to Roll & Pitch)
        const float cmdRoll = mouseInStage ? (nx * 65.0f) : 0.0f;
        const float cmdPitch = mouseInStage ? (-ny * 38.0f) : 2.5f;
        const float alpha = keel::clamp(fdt * 12.0f, 0.05f, 1.0f);
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
            st.pitchDeg = keel::clamp(st.pitchDeg + pitchRateDegPerSec * fdt, -85.0f, 85.0f);
            st.fdRollDeg = keel::clamp(sx * 45.0f, -60.0f, 60.0f);
            st.fdPitchDeg = keel::clamp(st.pitchDeg + pitchRateDegPerSec * 0.25f, -30.0f, 30.0f);
        }
    } else if (st.controlModeIndex == 2) {
        // Mode 2: Automated Autopilot Figure-8 Flight Pattern
        const float t = static_cast<float>(st.simTimeSec);
        const float targetRoll = 36.0f * std::sin(t * 0.55f);
        const float targetPitch = 3.0f + 9.5f * std::sin(t * 1.10f);
        st.fdRollDeg = 36.0f * std::sin((t + 0.45f) * 0.55f);
        st.fdPitchDeg = 3.0f + 9.5f * std::sin((t + 0.45f) * 1.10f);
        const float alpha = keel::clamp(fdt * 6.0f, 0.02f, 1.0f);
        st.rollDeg += (targetRoll - st.rollDeg) * alpha;
        st.pitchDeg += (targetPitch - st.pitchDeg) * alpha;
    }

    // Integrate realistic coupled air-data dynamics from Roll & Pitch
    const float rollRate = (st.rollDeg - prevRoll) / fdt;
    const float pitchRate = (st.pitchDeg - prevPitch) / fdt;

    // Vertical speed (FPM) driven by pitch angle and airspeed
    const float targetVsi = std::sin(keel::deg_to_rad(st.pitchDeg - 2.0f)) * st.airspeedKnots * 88.0f;
    st.verticalSpeedFpm += (targetVsi - st.verticalSpeedFpm) * keel::clamp(fdt * 6.0f, 0.05f, 1.0f);
    st.altitudeFeet = keel::clamp(st.altitudeFeet + (st.verticalSpeedFpm / 60.0f) * fdt, 0.0f, 50000.0f);
    st.radioAltitudeFeet = keel::clamp(st.altitudeFeet - 10650.0f, 20.0f, 2500.0f);

    // Airspeed responds to dive/climb + autothrottle restoring toward targetAirspeedBug
    const float gravityAccelKtPerSec = -std::sin(keel::deg_to_rad(st.pitchDeg - 2.0f)) * 18.0f;
    const float autoThrottleKtPerSec = (st.targetAirspeedBug - st.airspeedKnots) * 0.45f;
    st.airspeedKnots = keel::clamp(st.airspeedKnots + (gravityAccelKtPerSec + autoThrottleKtPerSec) * fdt, 95.0f, 380.0f);
    st.machNumber = keel::clamp(st.airspeedKnots / 348.0f, 0.15f, 0.99f);

    // Coordinated turn rate integrates magnetic heading
    const float turnRateDegPerSec =
        keel::clamp(std::tan(keel::deg_to_rad(keel::clamp(st.rollDeg, -70.0f, 70.0f))) * 4.2f, -18.0f, 18.0f);
    st.headingDeg = WrapHeadingDeg(st.headingDeg + turnRateDegPerSec * fdt);

    // Normal load factor (G) and inclinometer slip/skid ball
    const float bankCos = keel::max(0.35f, std::cos(keel::deg_to_rad(keel::clamp(st.rollDeg, -70.0f, 70.0f))));
    const float targetG = (1.0f / bankCos) + pitchRate * 0.035f;
    st.gLoad += (keel::clamp(targetG, -2.5f, 6.0f) - st.gLoad) * keel::clamp(fdt * 8.0f, 0.05f, 1.0f);

    const float targetSlip = keel::clamp(-rollRate * 0.008f, -0.85f, 0.85f);
    st.slipSkid += (targetSlip - st.slipSkid) * keel::clamp(fdt * 7.0f, 0.05f, 1.0f);
    st.aoaDeg = keel::clamp(2.0f + (st.gLoad - 1.0f) * 2.4f, -4.0f, 14.0f);
    st.sideslipDeg = st.slipSkid * 3.5f;

    // Smoothly animate ILS Localizer & Glideslope deviation relative to target bugs
    st.locDeviationDots = keel::clamp(std::sin(static_cast<float>(st.simTimeSec) * 0.35f) * 0.8f - st.rollDeg * 0.02f, -2.2f, 2.2f);
    st.gsDeviationDots = keel::clamp((st.targetAltitudeBug - st.altitudeFeet) / 250.0f, -2.2f, 2.2f);
}

} // namespace

void RenderAttitudeInspector(guikit::Context& ui, DemoContext& /*ctx*/) {
    static constexpr keel::StaticArray<keel::StringRef, 4> kControlModes = {
        "Mouse Follow (Direct)",
        "Virtual Stick (FBW 360°)",
        "Autopilot Figure-8",
        "Manual Sliders"
    };
    static constexpr keel::StaticArray<keel::StringRef, 3> kDisplayModes = {
        "Full PFD (Primary Flight Display)",
        "Compact ADI (No Side Tapes)",
        "HUD Overlay (Collimator)"
    };
    static constexpr keel::StaticArray<keel::StringRef, 4> kViewportModes = {
        "Auto (By Display Mode)",
        "Full-Bleed (Edge-to-Edge)",
        "Rounded Dome (Classic Bezel)",
        "Circular Sphere"
    };
    static constexpr keel::StaticArray<keel::StringRef, 2> kRefModes = {
        "Western",
        "Russian KPPI"
    };
    static constexpr keel::StaticArray<keel::StringRef, 3> kColorSchemes = {
        "Glass Cockpit (Sky/Earth)",
        "HUD Phosphor Green",
        "Tactical Dark"
    };
    static constexpr keel::StaticArray<keel::StringRef, 3> kFdStyles = {
        "Off",
        "V-Bar",
        "Crossbars"
    };
    static constexpr keel::StaticArray<keel::StringRef, 3> kAircraftSymbols = {
        "Gull-Wing (Garmin / Airbus)",
        "Classic W-Wing",
        "Russian KPPI Silhouette"
    };

    if (auto el = ui.Element({
        .label = "Flight Control Mode",
        .layout = guikit::ElementLayout::Top,
        .size = guikit::UiSize::Sm
    })) {
        ui.Select(s_att.controlModeIndex, kControlModes, {.size = guikit::UiSize::Sm});
    }

    if (auto el = ui.Element({
        .label = "Display Mode",
        .layout = guikit::ElementLayout::Top,
        .size = guikit::UiSize::Sm
    })) {
        ui.Select(s_att.displayModeIndex, kDisplayModes, {.size = guikit::UiSize::Sm});
    }

    if (auto el = ui.Element({
        .label = "Horizon Viewport",
        .layout = guikit::ElementLayout::Top,
        .size = guikit::UiSize::Sm
    })) {
        ui.Select(s_att.viewportModeIndex, kViewportModes, {.size = guikit::UiSize::Sm});
    }

    if (auto el = ui.Element({
        .label = "Horizon Reference Frame",
        .layout = guikit::ElementLayout::Top,
        .size = guikit::UiSize::Sm
    })) {
        if (ui.Segmented(s_att.referenceModeIndex, kRefModes, {.size = guikit::UiSize::Sm}).changed) {
            if (s_att.referenceModeIndex == 1) {
                s_att.aircraftSymbolIndex = 2; // Russian KPPI silhouette
            } else if (s_att.aircraftSymbolIndex == 2) {
                s_att.aircraftSymbolIndex = 0;
            }
        }
    }

    if (auto el = ui.Element({
        .label = "Color Scheme",
        .layout = guikit::ElementLayout::Top,
        .size = guikit::UiSize::Sm
    })) {
        ui.Select(s_att.colorSchemeIndex, kColorSchemes, {.size = guikit::UiSize::Sm});
    }

    if (auto el = ui.Element({
        .label = "Flight Director (FD)",
        .layout = guikit::ElementLayout::Top,
        .size = guikit::UiSize::Sm
    })) {
        ui.Segmented(s_att.fdStyleIndex, kFdStyles, {.size = guikit::UiSize::Sm});
    }

    if (auto el = ui.Element({
        .label = "Aircraft Reference Symbol",
        .layout = guikit::ElementLayout::Top,
        .size = guikit::UiSize::Sm
    })) {
        ui.Select(s_att.aircraftSymbolIndex, kAircraftSymbols, {.size = guikit::UiSize::Sm});
    }

    ui.Heading({.label = "Attitude & Air Data", .level = guikit::HeadingLevel::H6, .separator = guikit::HeadingSeparator::Bottom});

    if (auto el = ui.Element({
        .label = "Roll / Bank Angle",
        .layout = guikit::ElementLayout::Top,
        .size = guikit::UiSize::Sm
    })) {
        if (ui.Slider(s_att.rollDeg, -180.0f, 180.0f, {
            .valueFormat = "%+.1f deg",
            .size = guikit::UiSize::Sm
        }).changed) {
            s_att.controlModeIndex = 3;
        }
    }

    if (auto el = ui.Element({
        .label = "Pitch Angle",
        .layout = guikit::ElementLayout::Top,
        .size = guikit::UiSize::Sm
    })) {
        if (ui.Slider(s_att.pitchDeg, -85.0f, 85.0f, {
            .valueFormat = "%+.1f deg",
            .size = guikit::UiSize::Sm
        }).changed) {
            s_att.controlModeIndex = 3;
        }
    }

    if (auto el = ui.Element({
        .label = "Magnetic Heading",
        .layout = guikit::ElementLayout::Top,
        .size = guikit::UiSize::Sm
    })) {
        ui.Slider(s_att.headingDeg, 0.0f, 359.0f, {
            .valueFormat = "%03.0f deg",
            .size = guikit::UiSize::Sm
        });
    }

    if (auto el = ui.Element({
        .label = "Airspeed (IAS)",
        .layout = guikit::ElementLayout::Top,
        .size = guikit::UiSize::Sm
    })) {
        ui.Slider(s_att.airspeedKnots, 80.0f, 380.0f, {
            .valueFormat = "%.0f kt",
            .size = guikit::UiSize::Sm
        });
    }

    if (auto el = ui.Element({
        .label = "Altitude",
        .layout = guikit::ElementLayout::Top,
        .size = guikit::UiSize::Sm
    })) {
        ui.Slider(s_att.altitudeFeet, 0.0f, 42000.0f, {
            .valueFormat = "%.0f ft",
            .size = guikit::UiSize::Sm
        });
    }

    if (auto el = ui.Element({
        .label = "Pitch Field of View",
        .layout = guikit::ElementLayout::Top,
        .size = guikit::UiSize::Sm
    })) {
        ui.Slider(s_att.fovPitchDeg, 30.0f, 80.0f, {
            .valueFormat = "%.0f deg",
            .size = guikit::UiSize::Sm
        });
    }

    ui.Heading({.label = "Symbology & Tapes", .level = guikit::HeadingLevel::H6, .separator = guikit::HeadingSeparator::Bottom});

    if (auto el = ui.Element({.label = "Pitch Ladder & Chevrons", .size = guikit::UiSize::Sm})) {
        ui.Switch(s_att.showPitchLadder, {.size = guikit::UiSize::Sm});
    }
    if (auto el = ui.Element({.label = "Bank Arc & Slip/Skid Ball", .size = guikit::UiSize::Sm})) {
        ui.Switch(s_att.showBankScale, {.size = guikit::UiSize::Sm});
    }
    if (auto el = ui.Element({.label = "Flight Path Vector (FPV)", .size = guikit::UiSize::Sm})) {
        ui.Switch(s_att.showFlightPathVector, {.size = guikit::UiSize::Sm});
    }
    if (auto el = ui.Element({.label = "ILS Localizer & Glideslope", .size = guikit::UiSize::Sm})) {
        ui.Switch(s_att.showIlsDiamonds, {.size = guikit::UiSize::Sm});
    }
    if (auto el = ui.Element({.label = "Speed & Altitude Tapes", .size = guikit::UiSize::Sm})) {
        ui.Switch(s_att.showSpeedAndAltTapes, {.size = guikit::UiSize::Sm});
    }
    if (auto el = ui.Element({.label = "Compass Heading Tape", .size = guikit::UiSize::Sm})) {
        ui.Switch(s_att.showHeadingTape, {.size = guikit::UiSize::Sm});
    }
    if (auto el = ui.Element({.label = "Flight Mode Annunciator (FMA)", .size = guikit::UiSize::Sm})) {
        ui.Switch(s_att.showFmaBar, {.size = guikit::UiSize::Sm});
    }
    if (auto el = ui.Element({.label = "Mouse Stick Vector Overlay", .size = guikit::UiSize::Sm})) {
        ui.Switch(s_att.showMouseStickOverlay, {.size = guikit::UiSize::Sm});
    }

    if (ui.Button({
        .text = "Level Wings",
        .icon = guikit::Icon::Refresh(),
        .color = guikit::StateColor::Secondary(),
        .width = guikit::SizeMode::Stretch,
        .size = guikit::UiSize::Sm
    })) {
        s_att.rollDeg = 0.0f;
        s_att.pitchDeg = 2.0f;
        s_att.verticalSpeedFpm = 0.0f;
        s_att.slipSkid = 0.0f;
    }
}

void RenderAttitudeStage(guikit::Context& ui, DemoContext& /*ctx*/) {
    StepAttitudeSimulation(ui, s_att);
    if (s_att.controlModeIndex != 3) {
        ui.RequestRepaint(2);
    }

    // Main Autonomous AttitudeView Widget
    s_att.lastResp = ui.AttitudeView({
        .displayMode = static_cast<guikit::AttitudeDisplayMode>(s_att.displayModeIndex),
        .viewportMode = static_cast<guikit::AttitudeViewportMode>(s_att.viewportModeIndex),
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
        .height = guikit::SizeMode::Stretch
    });
}

keel::String GenerateAttitudeCode(const DemoContext& /*ctx*/) {
    static constexpr keel::StaticArray<const char*, 3> kModeEnums = {
        "AttitudeDisplayMode::FullPFD",
        "AttitudeDisplayMode::CompactADI",
        "AttitudeDisplayMode::HudOverlay"
    };
    static constexpr keel::StaticArray<const char*, 4> kViewportEnums = {
        "AttitudeViewportMode::Auto",
        "AttitudeViewportMode::FullBleed",
        "AttitudeViewportMode::RoundedDome",
        "AttitudeViewportMode::Circular"
    };
    static constexpr keel::StaticArray<const char*, 2> kRefEnums = {
        "HorizonReferenceMode::WesternMovingHorizon",
        "HorizonReferenceMode::EasternMovingAircraft"
    };
    static constexpr keel::StaticArray<const char*, 3> kSchemeEnums = {
        "AttitudeColorScheme::GlassCockpit",
        "AttitudeColorScheme::HudGreen",
        "AttitudeColorScheme::TacticalDark"
    };
    static constexpr keel::StaticArray<const char*, 3> kFdEnums = {
        "FlightDirectorStyle::Off",
        "FlightDirectorStyle::SingleCueVBar",
        "FlightDirectorStyle::DualCueCrossbars"
    };

    return keel::format(
        "// Avionics Primary Flight Display (PFD) & Artificial Horizon Widget\n"
        "auto resp = ui.AttitudeView({{\n"
        "    .displayMode          = guikit::{},\n"
        "    .viewportMode         = guikit::{},\n"
        "    .referenceMode        = guikit::{},\n"
        "    .colorScheme          = guikit::{},\n"
        "    .flightDirectorStyle  = guikit::{},\n"
        "    .rollDeg              = {:.1f}f,\n"
        "    .pitchDeg             = {:.1f}f,\n"
        "    .headingDeg           = {:.1f}f,\n"
        "    .airspeedKnots        = {:.1f}f,\n"
        "    .machNumber           = {:.2f}f,\n"
        "    .altitudeFeet         = {:.0f}f,\n"
        "    .verticalSpeedFpm     = {:.0f}f,\n"
        "    .slipSkid             = {:.2f}f,\n"
        "    .gLoad                = {:.2f}f,\n"
        "    .showFlightPathVector = {},\n"
        "    .showIlsDiamonds      = {},\n"
        "    .fovPitchDegPtr       = &fovPitchDeg,\n"
        "    .height               = guikit::SizeMode::Stretch\n"
        "}});\n"
        "if (resp.cursorInsideAdi) {{\n"
        "    // Mouse stick telemetry: roll = {:.1f} deg, pitch = {:.1f} deg\n"
        "}}\n",
        kModeEnums[static_cast<keel::usize>(keel::clamp(s_att.displayModeIndex, 0, 2))],
        kViewportEnums[static_cast<keel::usize>(keel::clamp(s_att.viewportModeIndex, 0, 3))],
        kRefEnums[static_cast<keel::usize>(keel::clamp(s_att.referenceModeIndex, 0, 1))],
        kSchemeEnums[static_cast<keel::usize>(keel::clamp(s_att.colorSchemeIndex, 0, 2))],
        kFdEnums[static_cast<keel::usize>(keel::clamp(s_att.fdStyleIndex, 0, 2))],
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
}

} // namespace guikit::demo
