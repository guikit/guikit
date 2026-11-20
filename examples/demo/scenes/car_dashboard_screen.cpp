#include "keel/math.h"
#include "../demo_context.hpp"

namespace guikit::demo {

namespace {

struct CarDashboardDemoState {
    bool initialized{false};
    keel::i32 controlModeIndex{0};        // 0 = Mouse Drive (Gas/Brake/Steer), 1 = Highway ADAS Cruise, 2 = Track Hot Lap, 3 = Manual Sliders
    keel::i32 layoutModeIndex{0};         // 0 = DualDial, 1 = TrackCentral, 2 = ModernWide
    keel::i32 powertrainIndex{0};         // 0 = IceTurbo, 1 = ElectricDualMotor
    keel::i32 driveThemeIndex{0};         // 0 = ComfortCyan, 1 = SportCrimson, 2 = EcoEmerald, 3 = TrackGold
    guikit::ClusterCenterMfd centerMfd{guikit::ClusterCenterMfd::AdasLaneRadar};
    keel::i32 speedUnitIndex{0};          // 0 = Kmh, 1 = Mph
    keel::i32 gearSelectorIndex{3};       // 0=P, 1=R, 2=N, 3=D, 4=S, 5=M

    // Primary Vehicle Telemetry
    float speedKmh{98.0f};
    float maxSpeedKmh{300.0f};
    float cruiseTargetKmh{120.0f};
    bool cruiseSet{true};
    float rpm{2650.0f};
    float redlineRpm{7000.0f};
    float maxRpm{8500.0f};
    keel::i32 currentGear{5};
    float evPowerKw{42.0f};
    float throttlePct{20.0f};
    float brakePct{0.0f};
    float steeringAngleDeg{0.0f};
    float boostBar{0.25f};

    // Energy, Thermal & Trip
    float fuelOrSocPct{78.0f};
    float estimatedRangeKm{450.0f};
    float coolantOrBattTempC{90.0f};
    float oilTempC{98.0f};
    float outsideTempC{19.0f};
    double odometerKm{18426.2};
    float tripDistanceKm{74.8f};
    float avgConsumption{9.2f};

    // Dynamics, ADAS & TPMS
    float latG{0.0f};
    float lonG{0.0f};
    float peakG{0.42f};
    float frontTorquePct{32.0f};
    float adasLeadDistanceM{42.0f};
    float adasLeadSpeedKmh{102.0f};
    bool adasLeadDetected{true};
    keel::StaticArray<float, 4> tirePressureBar{2.4f, 2.4f, 2.6f, 2.6f};
    keel::StaticArray<float, 4> tireTempC{44.0f, 45.0f, 48.0f, 48.0f};

    // Visibility & Telltale Flags
    bool showShiftLights{true};
    bool showTelltaleBar{true};
    bool showBottomTripBar{true};
    bool showMouseDriveOverlay{true};

    bool ttLeftTurn{false};
    bool ttRightTurn{false};
    bool ttHazard{false};
    bool ttLowBeam{true};
    bool ttHighBeam{false};
    bool ttFogLight{false};
    bool ttParkBrake{false};
    bool ttEscActive{false};
    bool ttTpmsWarning{false};
    bool ttAdasCruise{true};
    bool ttLaneKeep{true};

    double simTimeSec{0.0};
    keel::u64 lastWallNs{0};
    guikit::CarClusterViewResponse lastResp{};
};

CarDashboardDemoState s_car{};

// 8-speed DCT gear speed ratios (approximate km/h per 1000 RPM)
constexpr keel::StaticArray<float, 8> kGearKmhPer1000Rpm = {
    8.2f, 13.5f, 19.2f, 25.4f, 32.0f, 39.5f, 47.5f, 56.0f
};

void ComputeAutomaticGearAndRpm(CarDashboardDemoState& st, bool sportShift) noexcept {
    if (st.gearSelectorIndex == 0 || st.gearSelectorIndex == 2) {
        // Park or Neutral
        st.currentGear = 1;
        const float targetRpm = 820.0f + st.throttlePct * 58.0f;
        st.rpm += (targetRpm - st.rpm) * 0.18f;
        return;
    }

    const float upshiftRpm = sportShift ? (st.redlineRpm - 180.0f) : (2600.0f + st.throttlePct * 42.0f);
    const float downshiftRpm = sportShift ? 2800.0f : 1450.0f;

    if (st.gearSelectorIndex != 5) { // Not Manual mode -> automatic shift logic
        keel::i32 g = keel::clamp(st.currentGear, 1, 8);
        float ratio = kGearKmhPer1000Rpm[static_cast<keel::usize>(g - 1)];
        float calcRpm = keel::max(820.0f, (st.speedKmh / ratio) * 1000.0f);

        while (g < 8 && calcRpm > upshiftRpm) {
            ++g;
            ratio = kGearKmhPer1000Rpm[static_cast<keel::usize>(g - 1)];
            calcRpm = keel::max(820.0f, (st.speedKmh / ratio) * 1000.0f);
        }
        while (g > 1 && calcRpm < downshiftRpm && st.speedKmh > 6.0f) {
            --g;
            ratio = kGearKmhPer1000Rpm[static_cast<keel::usize>(g - 1)];
            calcRpm = keel::max(820.0f, (st.speedKmh / ratio) * 1000.0f);
        }
        st.currentGear = g;
    }

    const keel::i32 effGear = keel::clamp(st.currentGear, 1, 8);
    const float ratio = kGearKmhPer1000Rpm[static_cast<keel::usize>(effGear - 1)];
    const float wheelRpm = (st.speedKmh / ratio) * 1000.0f;
    const float targetRpm = keel::clamp(keel::max(820.0f + st.throttlePct * 6.0f, wheelRpm), 800.0f, st.maxRpm);
    st.rpm += (targetRpm - st.rpm) * 0.25f;
}

void StepCarSimulation(guikit::Context& ui, CarDashboardDemoState& st) {
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
        // Manual Sliders mode: keep peakG updated and keep road/telltale animations running
        st.peakG = keel::max(st.peakG, keel::hypot(st.latG, st.lonG));
        ui.RequestRepaint(2);
        return;
    }

    const float prevSpeedMps = st.speedKmh / 3.6f;

    if (st.controlModeIndex == 0) {
        // Mode 0: Interactive Mouse Drive (Hover cluster: Up = Throttle, Down = Brake/Regen, Left/Right = Steer)
        if (st.lastResp.cursorInside) {
            const float targetSteer = st.lastResp.cursorSteerDeg;
            const float targetThr = st.lastResp.cursorThrottlePct;
            const float targetBrk = st.lastResp.cursorBrakePct;

            st.steeringAngleDeg += (targetSteer - st.steeringAngleDeg) * keel::clamp(fdt * 12.0f, 0.05f, 1.0f);
            st.throttlePct += (targetThr - st.throttlePct) * keel::clamp(fdt * 10.0f, 0.05f, 1.0f);
            st.brakePct += (targetBrk - st.brakePct) * keel::clamp(fdt * 12.0f, 0.05f, 1.0f);
        } else {
            // Smoothly relax to highway cruise when mouse leaves the cluster
            st.steeringAngleDeg += (0.0f - st.steeringAngleDeg) * keel::clamp(fdt * 5.0f, 0.02f, 1.0f);
            const float cruiseErr = st.cruiseTargetKmh - st.speedKmh;
            const float autoThr = keel::clamp(18.0f + cruiseErr * 1.2f, 0.0f, 45.0f);
            st.throttlePct += (autoThr - st.throttlePct) * keel::clamp(fdt * 4.0f, 0.02f, 1.0f);
            st.brakePct += (0.0f - st.brakePct) * keel::clamp(fdt * 6.0f, 0.02f, 1.0f);
        }

        // Integrate longitudinal vehicle dynamics
        const float propAccelMps2 = (st.throttlePct * 0.01f) * 8.2f * (1.0f - 0.35f * (st.speedKmh / st.maxSpeedKmh));
        const float brakeDecelMps2 = (st.brakePct * 0.01f) * 11.5f;
        const float aeroDragMps2 = 0.18f + 0.00022f * st.speedKmh * st.speedKmh;
        const float netAccelMps2 = propAccelMps2 - brakeDecelMps2 - aeroDragMps2;

        st.speedKmh = keel::clamp(st.speedKmh + netAccelMps2 * 3.6f * fdt, 0.0f, st.maxSpeedKmh - 4.0f);
        ComputeAutomaticGearAndRpm(st, st.gearSelectorIndex == 4 || st.driveThemeIndex == 1 || st.driveThemeIndex == 3);

        // ADAS radar lead car distance modulation
        st.adasLeadSpeedKmh = 105.0f + 12.0f * keel::sin(static_cast<float>(st.simTimeSec) * 0.45f);
        const float relMps = (st.adasLeadSpeedKmh - st.speedKmh) / 3.6f;
        st.adasLeadDistanceM = keel::clamp(st.adasLeadDistanceM + relMps * fdt, 12.0f, 95.0f);
        if (st.adasLeadDistanceM >= 94.0f || st.adasLeadDistanceM <= 13.0f) {
            st.adasLeadDistanceM = 45.0f;
        }
    } else if (st.controlModeIndex == 1) {
        // Mode 1: Highway ADAS Cruise (Smooth adaptive cruise & gentle lane weaves)
        const float t = static_cast<float>(st.simTimeSec);
        const float targetSpd = st.cruiseTargetKmh + 14.0f * keel::sin(t * 0.55f);
        st.speedKmh += (targetSpd - st.speedKmh) * keel::clamp(fdt * 2.5f, 0.02f, 1.0f);
        st.steeringAngleDeg = 9.5f * keel::sin(t * 0.85f);
        st.throttlePct = keel::clamp(22.0f + 18.0f * keel::cos(t * 0.55f), 5.0f, 65.0f);
        st.brakePct = (keel::cos(t * 0.55f) < -0.55f) ? (-28.0f * (keel::cos(t * 0.55f) + 0.55f)) : 0.0f;
        st.adasLeadSpeedKmh = st.speedKmh + 4.0f * keel::cos(t * 0.7f);
        st.adasLeadDistanceM = 38.0f + 14.0f * keel::sin(t * 0.5f);
        ComputeAutomaticGearAndRpm(st, false);
    } else if (st.controlModeIndex == 2) {
        // Mode 2: Track Hot Lap (Full-throttle pulls to redline, apex braking & high cornering G)
        const float t = static_cast<float>(st.simTimeSec);
        const float lapPhase = keel::fmod(t, 14.0f);
        float targetSpd = 140.0f;
        if (lapPhase < 9.5f) {
            // Full-throttle straight pull
            targetSpd = 75.0f + (lapPhase / 9.5f) * 205.0f;
            st.throttlePct = 96.0f;
            st.brakePct = 0.0f;
            st.steeringAngleDeg = 6.0f * keel::sin(lapPhase * 1.4f);
        } else if (lapPhase < 11.8f) {
            // Heavy braking zone into chicane
            const float brkPhase = (lapPhase - 9.5f) / 2.3f;
            targetSpd = 280.0f - brkPhase * 185.0f;
            st.throttlePct = 0.0f;
            st.brakePct = 88.0f;
            st.steeringAngleDeg = -28.0f * keel::sin(brkPhase * keel::kPiF);
        } else {
            // Corner exit acceleration
            const float exitPhase = (lapPhase - 11.8f) / 2.2f;
            targetSpd = 95.0f + exitPhase * 55.0f;
            st.throttlePct = 78.0f;
            st.brakePct = 0.0f;
            st.steeringAngleDeg = 32.0f * (1.0f - exitPhase);
        }
        st.speedKmh += (targetSpd - st.speedKmh) * keel::clamp(fdt * 6.0f, 0.04f, 1.0f);
        st.adasLeadDistanceM = 24.0f + 12.0f * keel::sin(t * 1.1f);
        st.adasLeadSpeedKmh = st.speedKmh + 5.0f;
        ComputeAutomaticGearAndRpm(st, true);
    }

    // Derived Powertrain, G-Force, AWD Split & Trip Integration
    const float curSpeedMps = st.speedKmh / 3.6f;
    const float rawLonG = ((curSpeedMps - prevSpeedMps) / keel::max(0.005f, fdt)) / 9.81f;
    st.lonG += (keel::clamp(rawLonG, -1.45f, 1.35f) - st.lonG) * keel::clamp(fdt * 10.0f, 0.05f, 1.0f);

    const float speedFactor = keel::clamp(st.speedKmh / 140.0f, 0.1f, 1.4f);
    const float targetLatG = keel::clamp((st.steeringAngleDeg / 32.0f) * speedFactor * 1.05f, -1.45f, 1.45f);
    st.latG += (targetLatG - st.latG) * keel::clamp(fdt * 10.0f, 0.05f, 1.0f);
    st.peakG = keel::max(st.peakG, keel::hypot(st.latG, st.lonG));

    // Boost pressure & EV power
    const float targetBoost = -0.45f + (st.throttlePct * 0.01f) * 2.15f * keel::clamp(st.rpm / 5500.0f, 0.25f, 1.0f);
    st.boostBar += (targetBoost - st.boostBar) * keel::clamp(fdt * 8.0f, 0.05f, 1.0f);

    const float targetEvKw = (st.brakePct > 2.0f)
        ? (-st.brakePct * 1.45f)
        : (st.throttlePct * 4.35f - (1.0f - st.throttlePct * 0.01f) * 18.0f);
    st.evPowerKw += (targetEvKw - st.evPowerKw) * keel::clamp(fdt * 10.0f, 0.05f, 1.0f);

    // AWD torque distribution & ESC indicator
    st.frontTorquePct = keel::clamp(28.0f + keel::abs(st.steeringAngleDeg) * 0.45f + (st.throttlePct * 0.12f), 15.0f, 50.0f);
    st.ttEscActive = (keel::hypot(st.latG, st.lonG) > 1.08f);

    // Distance integration
    const double dKm = static_cast<double>(curSpeedMps * fdt) * 0.001;
    st.odometerKm += dKm;
    st.tripDistanceKm += static_cast<float>(dKm);

    ui.RequestRepaint(2);
}

} // namespace

void RenderCarDashboardInspector(guikit::Context& ui, DemoContext& /*ctx*/) {
    static constexpr keel::StaticArray<keel::StringRef, 4> kControlModes = {
        "Mouse Drive (Gas / Brake / Steer)",
        "Highway ADAS Cruise",
        "Track Hot Lap (Launch & Apex)",
        "Manual Sliders"
    };
    static constexpr keel::StaticArray<keel::StringRef, 3> kLayoutModes = {
        "Dual Dial Classic (Speed + Tach)",
        "Track Central (Motorsport GT)",
        "Modern Wide (Panoramic EV/ADAS)"
    };
    static constexpr keel::StaticArray<keel::StringRef, 2> kPowertrains = {
        "ICE Turbo",
        "EV Dual-Motor"
    };
    static constexpr keel::StaticArray<keel::StringRef, 4> kDriveThemes = {
        "Comfort (Cyan / Slate)",
        "Sport+ (Crimson / Carbon)",
        "Eco Pro (Emerald / Teal)",
        "Track (Motorsport Gold)"
    };
    static constexpr keel::StaticArray<keel::StringRef, 3> kMfdPages = {
        "ADAS 3D Lane & Radar",
        "TPMS & Chassis Telemetry",
        "G-Force & Powertrain Dynamics"
    };
    static constexpr keel::StaticArray<keel::StringRef, 2> kSpeedUnits = {
        "km/h (°C)",
        "mph (°F)"
    };
    static constexpr keel::StaticArray<keel::StringRef, 6> kGearSelectors = {
        "P (Park)", "R (Reverse)", "N (Neutral)", "D (Drive)", "S (Sport)", "M (Manual)"
    };

    keel::usize ctrlIdx = static_cast<keel::usize>(keel::clamp(s_car.controlModeIndex, 0, 3));
    if (auto el = ui.Element({.label = "Control / Simulation Mode"})) {
        if (ui.Select(ctrlIdx, kControlModes).changed) {
            s_car.controlModeIndex = static_cast<keel::i32>(ctrlIdx);
            if (s_car.controlModeIndex == 2) {
                s_car.driveThemeIndex = 1; // SportCrimson
                s_car.gearSelectorIndex = 4; // Sport
            }
        }
    }

    keel::usize layIdx = static_cast<keel::usize>(keel::clamp(s_car.layoutModeIndex, 0, 2));
    if (auto el = ui.Element({.label = "Cluster Architecture"})) {
        if (ui.Select(layIdx, kLayoutModes).changed) {
            s_car.layoutModeIndex = static_cast<keel::i32>(layIdx);
        }
    }

    keel::usize ptIdx = static_cast<keel::usize>(keel::clamp(s_car.powertrainIndex, 0, 1));
    if (auto el = ui.Element({.label = "Powertrain Mode"})) {
        if (ui.Segmented(ptIdx, kPowertrains).changed) {
            s_car.powertrainIndex = static_cast<keel::i32>(ptIdx);
            s_car.avgConsumption = (s_car.powertrainIndex == 1) ? 18.4f : 9.2f;
            s_car.coolantOrBattTempC = (s_car.powertrainIndex == 1) ? 31.0f : 90.0f;
        }
    }

    keel::usize thmIdx = static_cast<keel::usize>(keel::clamp(s_car.driveThemeIndex, 0, 3));
    if (auto el = ui.Element({.label = "Drive Mode Theme"})) {
        if (ui.Select(thmIdx, kDriveThemes).changed) {
            s_car.driveThemeIndex = static_cast<keel::i32>(thmIdx);
        }
    }

    keel::usize mfdIdx = static_cast<keel::usize>(s_car.centerMfd);
    if (auto el = ui.Element({.label = "Center MFD Screen"})) {
        if (ui.Select(mfdIdx, kMfdPages).changed) {
            s_car.centerMfd = static_cast<guikit::ClusterCenterMfd>(mfdIdx);
        }
    }

    keel::usize unitIdx = static_cast<keel::usize>(keel::clamp(s_car.speedUnitIndex, 0, 1));
    if (auto el = ui.Element({.label = "Measurement Units"})) {
        if (ui.Segmented(unitIdx, kSpeedUnits).changed) {
            s_car.speedUnitIndex = static_cast<keel::i32>(unitIdx);
        }
    }

    keel::usize gearSelIdx = static_cast<keel::usize>(keel::clamp(s_car.gearSelectorIndex, 0, 5));
    if (auto el = ui.Element({.label = "Transmission Selector"})) {
        if (ui.Select(gearSelIdx, kGearSelectors).changed) {
            s_car.gearSelectorIndex = static_cast<keel::i32>(gearSelIdx);
        }
    }

    ui.Heading({.label = "VEHICLE TELEMETRY", .level = guikit::HeadingLevel::H6, .separator = guikit::HeadingSeparator::Bottom});

    if (auto el = ui.Element({.label = "Ground Speed"})) {
        if (ui.Slider(s_car.speedKmh, 0.0f, s_car.maxSpeedKmh, {.valueFormat = "%.0f km/h"}).changed) {
            s_car.controlModeIndex = 3;
        }
    }
    if (s_car.powertrainIndex == 0) {
        if (auto el = ui.Element({.label = "Engine RPM"})) {
            if (ui.Slider(s_car.rpm, 0.0f, s_car.maxRpm, {.valueFormat = "%.0f RPM"}).changed) {
                s_car.controlModeIndex = 3;
            }
        }
    } else {
        if (auto el = ui.Element({.label = "EV Motor Power / Regen"})) {
            if (ui.Slider(s_car.evPowerKw, -150.0f, 450.0f, {.valueFormat = "%+.0f kW"}).changed) {
                s_car.controlModeIndex = 3;
            }
        }
    }

    if (auto el = ui.Element({.label = "ACC Cruise Target"})) {
        ui.Slider(s_car.cruiseTargetKmh, 30.0f, 250.0f, {.valueFormat = "%.0f km/h"});
    }
    if (auto el = ui.Element({.label = "Steering Wheel Angle"})) {
        if (ui.Slider(s_car.steeringAngleDeg, -45.0f, 45.0f, {.valueFormat = "%+.1f deg"}).changed) {
            s_car.controlModeIndex = 3;
        }
    }
    if (auto el = ui.Element({.label = "Fuel / Battery SoC"})) {
        ui.Slider(s_car.fuelOrSocPct, 0.0f, 100.0f, {.valueFormat = "%.0f %%"});
    }
    if (auto el = ui.Element({.label = "Radar Lead Car Distance"})) {
        ui.Slider(s_car.adasLeadDistanceM, 8.0f, 100.0f, {.valueFormat = "%.0f m"});
    }

    ui.Heading({.label = "TELLTALES & OVERLAYS", .level = guikit::HeadingLevel::H6, .separator = guikit::HeadingSeparator::Bottom});
    if (auto el = ui.Element({.label = "Top Shift-Lights LED Bar"})) {
        ui.Switch(s_car.showShiftLights);
    }
    if (auto el = ui.Element({.label = "ISO Telltale Status Ribbon"})) {
        ui.Switch(s_car.showTelltaleBar);
    }
    if (auto el = ui.Element({.label = "Bottom PRNDSM & Trip Bar"})) {
        ui.Switch(s_car.showBottomTripBar);
    }
    if (auto el = ui.Element({.label = "Interactive Mouse Drive Reticle"})) {
        ui.Switch(s_car.showMouseDriveOverlay);
    }
    if (auto el = ui.Element({.label = "Left Turn Signal"})) {
        ui.Switch(s_car.ttLeftTurn);
    }
    if (auto el = ui.Element({.label = "Right Turn Signal"})) {
        ui.Switch(s_car.ttRightTurn);
    }
    if (auto el = ui.Element({.label = "Hazard Warning Flashers"})) {
        ui.Switch(s_car.ttHazard);
    }
    if (auto el = ui.Element({.label = "High Beam Headlights"})) {
        ui.Switch(s_car.ttHighBeam);
    }
    if (auto el = ui.Element({.label = "TPMS Tire Alert"})) {
        ui.Switch(s_car.ttTpmsWarning);
    }

    if (ui.Button({
        .text = "Reset Peak G",
        .icon = guikit::Icon::Refresh(),
        .size = guikit::UiSize::Sm,
        .width = guikit::SizeMode::Stretch,
        .color = guikit::StateColor::Secondary()
    })) {
        s_car.peakG = 0.0f;
    }
}

void RenderCarDashboardStage(guikit::Context& ui, DemoContext& /*ctx*/) {
    StepCarSimulation(ui, s_car);

    const bool blinkOn = (keel::fmod(s_car.simTimeSec, 0.70) < 0.35);
    const bool autoLeftTurn = (s_car.controlModeIndex <= 2) && (s_car.steeringAngleDeg < -16.0f);
    const bool autoRightTurn = (s_car.controlModeIndex <= 2) && (s_car.steeringAngleDeg > 16.0f);

    // Main Autonomous CarClusterView Widget
    s_car.lastResp = ui.CarClusterView({
        .layoutMode = static_cast<guikit::ClusterLayoutMode>(s_car.layoutModeIndex),
        .powertrain = static_cast<guikit::PowertrainMode>(s_car.powertrainIndex),
        .driveTheme = static_cast<guikit::ClusterDriveTheme>(s_car.driveThemeIndex),
        .centerMfd = s_car.centerMfd,
        .speedUnit = static_cast<guikit::SpeedUnit>(s_car.speedUnitIndex),
        .gearSelector = static_cast<guikit::GearSelector>(s_car.gearSelectorIndex),
        .speedKmh = s_car.speedKmh,
        .maxSpeedKmh = s_car.maxSpeedKmh,
        .cruiseTargetKmh = s_car.cruiseTargetKmh,
        .cruiseSet = s_car.cruiseSet,
        .rpm = s_car.rpm,
        .redlineRpm = s_car.redlineRpm,
        .maxRpm = s_car.maxRpm,
        .currentGear = s_car.currentGear,
        .evPowerKw = s_car.evPowerKw,
        .throttlePct = s_car.throttlePct,
        .brakePct = s_car.brakePct,
        .steeringAngleDeg = s_car.steeringAngleDeg,
        .boostBar = s_car.boostBar,
        .fuelOrSocPct = s_car.fuelOrSocPct,
        .estimatedRangeKm = s_car.estimatedRangeKm,
        .coolantOrBattTempC = s_car.coolantOrBattTempC,
        .oilTempC = s_car.oilTempC,
        .outsideTempC = s_car.outsideTempC,
        .odometerKm = s_car.odometerKm,
        .tripDistanceKm = s_car.tripDistanceKm,
        .avgConsumption = s_car.avgConsumption,
        .latG = s_car.latG,
        .lonG = s_car.lonG,
        .peakG = s_car.peakG,
        .frontTorquePct = s_car.frontTorquePct,
        .adasLeadDistanceM = s_car.adasLeadDistanceM,
        .adasLeadSpeedKmh = s_car.adasLeadSpeedKmh,
        .adasLeadDetected = s_car.adasLeadDetected,
        .tirePressureBar = s_car.tirePressureBar,
        .tireTempC = s_car.tireTempC,
        .showShiftLights = s_car.showShiftLights,
        .showTelltaleBar = s_car.showTelltaleBar,
        .showBottomTripBar = s_car.showBottomTripBar,
        .showMouseDriveOverlay = s_car.showMouseDriveOverlay && (s_car.controlModeIndex == 0),
        .telltales = {
            .leftTurn = s_car.ttLeftTurn || autoLeftTurn,
            .rightTurn = s_car.ttRightTurn || autoRightTurn,
            .hazard = s_car.ttHazard,
            .lowBeam = s_car.ttLowBeam,
            .highBeam = s_car.ttHighBeam,
            .fogLight = s_car.ttFogLight,
            .parkBrake = s_car.ttParkBrake,
            .escActive = s_car.ttEscActive,
            .tpmsWarning = s_car.ttTpmsWarning,
            .adasCruiseActive = s_car.ttAdasCruise,
            .laneKeepActive = s_car.ttLaneKeep,
            .blinkPhaseOn = blinkOn
        },
        .manualGearPtr = &s_car.currentGear,
        .centerMfdPtr = &s_car.centerMfd,
        .height = guikit::SizeMode::Stretch
    });
}

keel::String GenerateCarDashboardCode(const DemoContext& /*ctx*/) {
    static constexpr keel::StaticArray<const char*, 3> kLayoutEnums = {
        "ClusterLayoutMode::DualDial",
        "ClusterLayoutMode::TrackCentral",
        "ClusterLayoutMode::ModernWide"
    };
    static constexpr keel::StaticArray<const char*, 2> kPtEnums = {
        "PowertrainMode::IceTurbo",
        "PowertrainMode::ElectricDualMotor"
    };
    static constexpr keel::StaticArray<const char*, 4> kThemeEnums = {
        "ClusterDriveTheme::ComfortCyan",
        "ClusterDriveTheme::SportCrimson",
        "ClusterDriveTheme::EcoEmerald",
        "ClusterDriveTheme::TrackGold"
    };
    static constexpr keel::StaticArray<const char*, 3> kMfdEnums = {
        "ClusterCenterMfd::AdasLaneRadar",
        "ClusterCenterMfd::TpmsChassis",
        "ClusterCenterMfd::GForceDynamics"
    };

    return keel::format(
        "// Automotive Digital Instrument Cluster & ADAS Cockpit Widget\n"
        "auto resp = ui.CarClusterView({{\n"
        "    .layoutMode       = guikit::{},\n"
        "    .powertrain       = guikit::{},\n"
        "    .driveTheme       = guikit::{},\n"
        "    .centerMfd        = guikit::{},\n"
        "    .speedKmh         = {:.1f}f,\n"
        "    .rpm              = {:.0f}f,\n"
        "    .currentGear      = {},\n"
        "    .evPowerKw        = {:+.1f}f,\n"
        "    .throttlePct      = {:.0f}f,\n"
        "    .brakePct         = {:.0f}f,\n"
        "    .steeringAngleDeg = {:+.1f}f,\n"
        "    .fuelOrSocPct     = {:.0f}f,\n"
        "    .latG             = {:+.2f}f,\n"
        "    .lonG             = {:+.2f}f,\n"
        "    .manualGearPtr    = &currentGear,\n"
        "    .centerMfdPtr     = &centerMfd,\n"
        "    .height           = guikit::SizeMode::Stretch\n"
        "}});\n",
        kLayoutEnums[static_cast<keel::usize>(keel::clamp(s_car.layoutModeIndex, 0, 2))],
        kPtEnums[static_cast<keel::usize>(keel::clamp(s_car.powertrainIndex, 0, 1))],
        kThemeEnums[static_cast<keel::usize>(keel::clamp(s_car.driveThemeIndex, 0, 3))],
        kMfdEnums[static_cast<keel::usize>(keel::clamp(static_cast<keel::i32>(s_car.centerMfd), 0, 2))],
        s_car.speedKmh,
        s_car.rpm,
        s_car.currentGear,
        s_car.evPowerKw,
        s_car.throttlePct,
        s_car.brakePct,
        s_car.steeringAngleDeg,
        s_car.fuelOrSocPct,
        s_car.latG,
        s_car.lonG
    );
}

} // namespace guikit::demo
