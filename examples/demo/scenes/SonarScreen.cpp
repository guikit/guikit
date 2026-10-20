#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>
#include <vector>
#include "../DemoContext.hpp"

namespace guikit::demo {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

struct DynamicContact {
    int id{0};
    std::string callsign{};
    SonarTargetClass classification{SonarTargetClass::Diver};
    // Cartesian position in meters relative to sensor (X = East, Y = North)
    float x{0.0f};
    float y{150.0f};
    float vx{0.5f};
    float vy{-0.8f};
    float depthMeters{6.5f};
    float snrDb{19.5f};
    float echoStrength{0.92f};
    float radialSpreadMeters{7.0f};
    float angularSpreadDeg{3.2f};
    std::vector<PolarPoint> trail{};
};

struct SonarDemoState {
    bool initialized{false};
    bool liveSimulation{true};
    int scenarioIndex{0};           // 0 = Harbor Defense DDS, 1 = FLS Shipwreck Inspection, 2 = Offshore Rig 360 Sweep
    int displayModeIndex{0};        // 0 = PPI_360, 1 = SectorWedge, 2 = BScan
    int scanModeIndex{0};           // 0 = MultibeamContinuous, 1 = RotatingSweep
    Colormap colormap{Colormap::Gold};

    float maxRangeMeters{500.0f};
    float headingDeg{0.0f};
    float sectorApertureDeg{120.0f};
    float gainDb{3.0f};
    float tvgAmount{0.58f};
    float clutterThreshold{0.14f};
    float persistence{0.84f};
    float sweepSpeedDegPerSec{95.0f};
    float sweepAngleDeg{0.0f};
    float rangeZoom{1.0f};

    bool showRangeRings{true};
    bool showBearingDial{true};
    bool showTargets{true};
    bool showTargetLabels{true};
    bool showColorbar{true};
    bool showEblVrm{true};
    bool showHudOverlay{true};
    bool guardZoneEnabled{true};

    float guardMinRange{90.0f};
    float guardMaxRange{340.0f};
    float guardStartBrg{315.0f};
    float guardEndBrg{55.0f};

    int selectedTargetId{1};
    float eblBearingDeg{22.0f};
    float vrmRangeMeters{210.0f};
    bool requestResetView{false};
    bool requestClearPersistence{false};

    uint32_t numBeams{180};
    uint32_t numRangeBins{256};
    std::vector<float> staticBathymetry{};
    std::vector<float> polarMatrix{};
    std::vector<DynamicContact> contacts{};
    std::vector<SonarTarget> targetViews{};

    double simTimeSec{0.0};
    double lastTrailSampleSec{0.0};
    std::chrono::steady_clock::time_point lastWallClock{};

    SonarViewResponse lastResp{};
};

SonarDemoState s_sonar{};

[[nodiscard]] inline float NormalizeDeg(float d) noexcept {
    float r = std::fmod(d, 360.0f);
    if (r < 0.0f) r += 360.0f;
    return r;
}

[[nodiscard]] inline float AngleDiffDeg(float a, float b) noexcept {
    return std::fmod(a - b + 540.0f, 360.0f) - 180.0f;
}

[[nodiscard]] inline float FastHashNoise(uint32_t x, uint32_t y, uint32_t seed) noexcept {
    uint32_t n = x * 374761393u + y * 668265263u + seed * 1442695041u;
    n = (n ^ (n >> 13u)) * 1274126177u;
    n = n ^ (n >> 16u);
    return static_cast<float>(n & 0xFFFFu) / 65535.0f;
}

void BuildStaticBathymetry(SonarDemoState& st) {
    const size_t total = static_cast<size_t>(st.numBeams) * st.numRangeBins;
    st.staticBathymetry.assign(total, 0.0f);

    for (uint32_t b = 0; b < st.numBeams; ++b) {
        const float brg = (static_cast<float>(b) / static_cast<float>(st.numBeams)) * 360.0f;
        for (uint32_t r = 0; r < st.numRangeBins; ++r) {
            const float rNorm = static_cast<float>(r) / static_cast<float>(st.numRangeBins - 1);
            const float rng = rNorm * st.maxRangeMeters;

            // Base seabed reverberation + gentle shoal ridges
            const float ridge1 = 0.07f * std::pow(std::max(0.0f, std::sin(rNorm * 14.0f + std::cos(brg * kDegToRad * 3.0f))), 2.0f);
            const float noise = 0.06f * FastHashNoise(b, r, 42u);
            float val = 0.04f + ridge1 + noise;

            // Harbor Breakwater wall between bearing 300..348 deg at ~260..310 m
            if (brg >= 296.0f && brg <= 350.0f) {
                const float wallR = 255.0f + (brg - 296.0f) * 1.15f;
                const float distToWall = std::abs(rng - wallR);
                if (distToWall < 16.0f) {
                    const float wallEcho = 0.86f * std::exp(-(distToWall * distToWall) / (2.0f * 5.2f * 5.2f));
                    val += wallEcho * (0.85f + 0.15f * FastHashNoise(b, r, 99u));
                } else if (rng > wallR + 14.0f) {
                    // Acoustic shadow zone behind the stone breakwater!
                    val *= 0.18f;
                }
            }

            // South-East Pier Pilings & Seafloor Pipeline along 065..105 deg
            if (brg >= 62.0f && brg <= 108.0f) {
                const float pierR = 175.0f + (brg - 62.0f) * 1.8f;
                const float distToPier = std::abs(rng - pierR);
                if (distToPier < 10.0f) {
                    const float pilingMod = 0.5f + 0.5f * std::cos((brg - 62.0f) * kDegToRad * 45.0f);
                    val += 0.72f * pilingMod * std::exp(-(distToPier * distToPier) / (2.0f * 3.8f * 3.8f));
                }
            }

            // Submerged wreck / reef anomaly at (145m, 012 deg)
            {
                const float dBrg = AngleDiffDeg(brg, 12.0f);
                const float dRng = rng - 142.0f;
                const float d2 = (dRng * dRng) / (11.0f * 11.0f) + (dBrg * dBrg) / (3.8f * 3.8f);
                if (d2 < 9.0f) {
                    val += 0.68f * std::exp(-0.5f * d2);
                }
            }

            st.staticBathymetry[static_cast<size_t>(b) * st.numRangeBins + r] = std::clamp(val, 0.0f, 1.0f);
        }
    }
}

void ApplyScenarioPreset(SonarDemoState& st, int presetIdx) {
    st.scenarioIndex = presetIdx;
    st.contacts.clear();

    if (presetIdx == 0) {
        // 0: Harbor Defense DDS (360 PPI or Sector)
        st.displayModeIndex = 0; // PPI_360
        st.scanModeIndex = 0;    // MultibeamContinuous
        st.colormap = Colormap::Gold;
        st.maxRangeMeters = 500.0f;
        st.headingDeg = 0.0f;
        st.sectorApertureDeg = 120.0f;
        st.gainDb = 3.0f;
        st.tvgAmount = 0.58f;
        st.clutterThreshold = 0.14f;
        st.persistence = 0.84f;
        st.guardZoneEnabled = true;
        st.guardMinRange = 90.0f;
        st.guardMaxRange = 340.0f;
        st.guardStartBrg = 315.0f;
        st.guardEndBrg = 55.0f;
    } else if (presetIdx == 1) {
        // 1: Forward-Looking Multibeam Sector Wedge (DDS / FLS)
        st.displayModeIndex = 1; // SectorWedge
        st.scanModeIndex = 0;    // MultibeamContinuous
        st.colormap = Colormap::Jet;
        st.maxRangeMeters = 400.0f;
        st.headingDeg = 8.0f;
        st.sectorApertureDeg = 120.0f;
        st.gainDb = 4.5f;
        st.tvgAmount = 0.65f;
        st.clutterThreshold = 0.15f;
        st.persistence = 0.86f;
        st.guardZoneEnabled = true;
        st.guardMinRange = 80.0f;
        st.guardMaxRange = 290.0f;
        st.guardStartBrg = 325.0f;
        st.guardEndBrg = 52.0f;
    } else {
        // 2: Offshore Platform 360 Rotating Radar / Sonar Sweep
        st.displayModeIndex = 0; // PPI_360
        st.scanModeIndex = 1;    // RotatingSweep
        st.colormap = Colormap::Green;
        st.maxRangeMeters = 500.0f;
        st.headingDeg = 0.0f;
        st.sectorApertureDeg = 120.0f;
        st.gainDb = 4.0f;
        st.tvgAmount = 0.52f;
        st.clutterThreshold = 0.16f;
        st.persistence = 0.90f;
        st.guardZoneEnabled = true;
        st.guardMinRange = 110.0f;
        st.guardMaxRange = 360.0f;
        st.guardStartBrg = 290.0f;
        st.guardEndBrg = 70.0f;
    }

    // Populate dynamic underwater & surface contacts
    st.contacts.push_back(DynamicContact{
        .id = 1,
        .callsign = "Target-A",
        .classification = SonarTargetClass::Diver,
        .x = 68.0f,
        .y = 215.0f,
        .vx = -0.45f,
        .vy = -1.05f,
        .depthMeters = 6.8f,
        .snrDb = 21.4f,
        .echoStrength = 0.95f,
        .radialSpreadMeters = 6.5f,
        .angularSpreadDeg = 3.0f
    });
    st.contacts.push_back(DynamicContact{
        .id = 2,
        .callsign = "Target-B",
        .classification = SonarTargetClass::Diver,
        .x = -75.0f,
        .y = 195.0f,
        .vx = 0.58f,
        .vy = -0.82f,
        .depthMeters = 8.4f,
        .snrDb = 18.9f,
        .echoStrength = 0.90f,
        .radialSpreadMeters = 6.5f,
        .angularSpreadDeg = 3.0f
    });
    st.contacts.push_back(DynamicContact{
        .id = 3,
        .callsign = "Target-C",
        .classification = SonarTargetClass::UUV,
        .x = 135.0f,
        .y = 140.0f,
        .vx = -1.65f,
        .vy = 1.25f,
        .depthMeters = 14.5f,
        .snrDb = 26.2f,
        .echoStrength = 0.98f,
        .radialSpreadMeters = 8.5f,
        .angularSpreadDeg = 3.6f
    });
    st.contacts.push_back(DynamicContact{
        .id = 4,
        .callsign = "PATROL-07",
        .classification = SonarTargetClass::SurfaceCraft,
        .x = -195.0f,
        .y = -110.0f,
        .vx = 2.40f,
        .vy = 1.90f,
        .depthMeters = 1.2f,
        .snrDb = 31.0f,
        .echoStrength = 1.0f,
        .radialSpreadMeters = 11.0f,
        .angularSpreadDeg = 4.2f
    });
    st.contacts.push_back(DynamicContact{
        .id = 5,
        .callsign = "BIO-SHL4",
        .classification = SonarTargetClass::Biologic,
        .x = 35.0f,
        .y = -180.0f,
        .vx = -0.75f,
        .vy = 0.35f,
        .depthMeters = 11.0f,
        .snrDb = 14.2f,
        .echoStrength = 0.76f,
        .radialSpreadMeters = 15.0f,
        .angularSpreadDeg = 6.5f
    });

    st.selectedTargetId = 1;
    st.requestClearPersistence = true;
    BuildStaticBathymetry(st);
}

void StepSonarSimulation(SonarDemoState& st) {
    const auto now = std::chrono::steady_clock::now();
    if (!st.initialized) {
        st.initialized = true;
        st.lastWallClock = now;
        ApplyScenarioPreset(st, 0);
    }

    double dt = std::chrono::duration<double>(now - st.lastWallClock).count();
    st.lastWallClock = now;
    dt = std::clamp(dt, 0.0, 0.08);

    if (st.liveSimulation) {
        if (dt < 0.004) dt = 0.016; // deterministic fallback for headless/unit-test frames
        st.simTimeSec += dt;
        st.sweepAngleDeg = NormalizeDeg(st.sweepAngleDeg + static_cast<float>(dt) * st.sweepSpeedDegPerSec);

        const bool recordTrail = (st.simTimeSec - st.lastTrailSampleSec) >= 0.45;
        if (recordTrail) {
            st.lastTrailSampleSec = st.simTimeSec;
        }

        // Update dynamic contact kinematics (scaled 5x speed so motion is visually engaging in real time)
        const float kinematicScale = 5.5f;
        for (auto& c : st.contacts) {
            c.x += c.vx * static_cast<float>(dt) * kinematicScale;
            c.y += c.vy * static_cast<float>(dt) * kinematicScale;

            // Gentle sinusoidal maneuvering & boundary turnaround
            const float dist = std::sqrt(c.x * c.x + c.y * c.y);
            if (dist < 45.0f || dist > st.maxRangeMeters * 0.88f) {
                c.vx = -c.vx;
                c.vy = -c.vy;
            }

            if (recordTrail) {
                const float rMeters = std::sqrt(c.x * c.x + c.y * c.y);
                const float brgDeg = NormalizeDeg(std::atan2(c.x, c.y) * kRadToDeg);
                c.trail.push_back(PolarPoint{rMeters, brgDeg});
                if (c.trail.size() > 18) {
                    c.trail.erase(c.trail.begin());
                }
            }
        }
    }

    // Rebuild `targetViews` and synthesize live `polarMatrix`
    st.targetViews.clear();
    st.targetViews.reserve(st.contacts.size());

    const size_t totalCells = static_cast<size_t>(st.numBeams) * st.numRangeBins;
    if (st.staticBathymetry.size() != totalCells) {
        BuildStaticBathymetry(st);
    }
    st.polarMatrix = st.staticBathymetry;

    // Add time-varying acoustic speckle noise
    const uint32_t frameSeed = static_cast<uint32_t>(st.simTimeSec * 30.0);
    for (size_t i = 0; i < totalCells; ++i) {
        const float speckle = (FastHashNoise(static_cast<uint32_t>(i), frameSeed, 7u) - 0.5f) * 0.07f;
        st.polarMatrix[i] = std::clamp(st.polarMatrix[i] + speckle, 0.0f, 1.0f);
    }

    // Inject moving target acoustic blooms & wakes into `polarMatrix`
    for (const auto& c : st.contacts) {
        const float rMeters = std::sqrt(c.x * c.x + c.y * c.y);
        const float brgDeg = NormalizeDeg(std::atan2(c.x, c.y) * kRadToDeg);
        const float speed = std::sqrt(c.vx * c.vx + c.vy * c.vy);
        const float courseDeg = NormalizeDeg(std::atan2(c.vx, c.vy) * kRadToDeg);

        st.targetViews.push_back(SonarTarget{
            .id = c.id,
            .callsign = c.callsign,
            .classification = c.classification,
            .rangeMeters = rMeters,
            .bearingDeg = brgDeg,
            .speedMetersPerSec = speed,
            .courseDeg = courseDeg,
            .depthMeters = c.depthMeters,
            .snrDb = c.snrDb,
            .trail = c.trail
        });

        // Paint 2D polar Gaussian signature into `polarMatrix`
        const float invSigR2 = 1.0f / std::max(4.0f, c.radialSpreadMeters * c.radialSpreadMeters);
        const float invSigB2 = 1.0f / std::max(1.0f, c.angularSpreadDeg * c.angularSpreadDeg);
        for (uint32_t b = 0; b < st.numBeams; ++b) {
            const float beamBrg = (static_cast<float>(b) / static_cast<float>(st.numBeams)) * 360.0f;
            const float dBrg = AngleDiffDeg(beamBrg, brgDeg);
            if (std::abs(dBrg) > c.angularSpreadDeg * 3.2f) continue;

            float* row = st.polarMatrix.data() + static_cast<size_t>(b) * st.numRangeBins;
            for (uint32_t r = 0; r < st.numRangeBins; ++r) {
                const float binRng = (static_cast<float>(r) / static_cast<float>(st.numRangeBins - 1)) * st.maxRangeMeters;
                const float dRng = binRng - rMeters;
                if (std::abs(dRng) > c.radialSpreadMeters * 3.2f) continue;

                const float g = std::exp(-0.5f * (dRng * dRng * invSigR2 + dBrg * dBrg * invSigB2));
                row[r] = std::min(1.0f, row[r] + c.echoStrength * g);
            }
        }

        // Paint faint wake trail echoes along historical trail points
        for (size_t ti = 0; ti < c.trail.size(); ++ti) {
            const float wakeWeight = 0.28f * (static_cast<float>(ti + 1) / static_cast<float>(c.trail.size() + 1));
            const float trR = c.trail[ti].rangeMeters;
            const float trB = c.trail[ti].bearingDeg;
            const uint32_t bCenter = static_cast<uint32_t>(NormalizeDeg(trB) / 360.0f * st.numBeams) % st.numBeams;
            const uint32_t rCenter = static_cast<uint32_t>(std::clamp(trR / st.maxRangeMeters, 0.0f, 0.999f) * st.numRangeBins);
            st.polarMatrix[static_cast<size_t>(bCenter) * st.numRangeBins + rCenter] =
                std::min(1.0f, st.polarMatrix[static_cast<size_t>(bCenter) * st.numRangeBins + rCenter] + wakeWeight);
        }
    }
}

} // namespace

void RenderSonarInspector(guikit::Context& ui, DemoContext& /*ctx*/) {
    static constexpr std::array<std::string_view, 3> kDisplayModeItems = {
        "PPI 360°",
        "Sector Wedge",
        "B-Scan"
    };
    static constexpr std::array<std::string_view, 2> kScanModeItems = {
        "Multibeam",
        "Rotating Sweep"
    };

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Live Simulation"
    })) {
        ui.Size(guikit::UiSize::Sm).Switch(s_sonar.liveSimulation);
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Display Geometry",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Select(s_sonar.displayModeIndex, kDisplayModeItems);
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Scan Mode",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Select(s_sonar.scanModeIndex, kScanModeItems);
    }

    int cmapIdx = static_cast<int>(s_sonar.colormap);
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Colormap",
        .layout = guikit::ElementLayout::Top
    })) {
        if (ui.Size(guikit::UiSize::Sm).Select(cmapIdx, guikit::GetAllColormapNames()).changed) {
            s_sonar.colormap = static_cast<guikit::Colormap>(cmapIdx);
        }
    }

    ui.Size(guikit::UiSize::Xs).Heading({.label = "Processing", .divider = true});

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Gain",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Slider(s_sonar.gainDb, -15.0f, 25.0f, {
            .valueFormat = "%+.1f dB"
        });
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "TVG",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Slider(s_sonar.tvgAmount, 0.0f, 1.0f, {
            .valueFormat = "%.2f"
        });
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Clutter Floor",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Slider(s_sonar.clutterThreshold, 0.0f, 0.65f, {
            .valueFormat = "%.2f"
        });
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Persistence",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Slider(s_sonar.persistence, 0.10f, 0.96f, {
            .valueFormat = "%.2f"
        });
    }

    if (s_sonar.displayModeIndex != 0) {
        if (auto el = ui.Size(guikit::UiSize::Sm).Element({
            .label = "Sector Aperture",
            .layout = guikit::ElementLayout::Top
        })) {
            ui.Size(guikit::UiSize::Sm).Slider(s_sonar.sectorApertureDeg, 60.0f, 180.0f, {
                .valueFormat = "%.0f deg"
            });
        }
    }

    ui.Size(guikit::UiSize::Xs).Heading({.label = "Overlays", .divider = true});

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "Guard Zone"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_sonar.guardZoneEnabled);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "Targets"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_sonar.showTargets);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "Target Labels"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_sonar.showTargetLabels);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "Range Rings"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_sonar.showRangeRings);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "Bearing Dial"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_sonar.showBearingDial);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "EBL / VRM"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_sonar.showEblVrm);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "Colorbar"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_sonar.showColorbar);
    }
}

void RenderSonarStage(guikit::Context& ui, DemoContext& /*ctx*/) {
    StepSonarSimulation(s_sonar);
    if (s_sonar.liveSimulation) {
        ui.RequestRepaint(2);
    }

    // Top Tactical Control Strip
    if (auto tb = ui.Size(guikit::UiSize::Sm).Toolbar()) {
        if (tb.ToolButton({
            .text = s_sonar.liveSimulation ? "LIVE PING" : "PAUSED",
            .icon = s_sonar.liveSimulation ? guikit::Icon::Pause() : guikit::Icon::Play(),
            .color = s_sonar.liveSimulation ? guikit::StateColor::Success() : guikit::StateColor::Warning(),
            .active = s_sonar.liveSimulation
        })) {
            s_sonar.liveSimulation = !s_sonar.liveSimulation;
        }

        tb.Separator();

        if (tb.ToolButton({
            .text = "PPI 360°",
            .color = (s_sonar.displayModeIndex == 0) ? guikit::StateColor::Primary() : guikit::StateColor::Ghost(),
            .active = (s_sonar.displayModeIndex == 0)
        })) {
            s_sonar.displayModeIndex = 0;
        }
        if (tb.ToolButton({
            .text = "Sector Wedge",
            .color = (s_sonar.displayModeIndex == 1) ? guikit::StateColor::Primary() : guikit::StateColor::Ghost(),
            .active = (s_sonar.displayModeIndex == 1)
        })) {
            s_sonar.displayModeIndex = 1;
        }
        if (tb.ToolButton({
            .text = "B-Scan",
            .color = (s_sonar.displayModeIndex == 2) ? guikit::StateColor::Primary() : guikit::StateColor::Ghost(),
            .active = (s_sonar.displayModeIndex == 2)
        })) {
            s_sonar.displayModeIndex = 2;
        }

        tb.Separator();

        if (tb.ToolButton({
            .text = "Lock Target-A",
            .color = (s_sonar.selectedTargetId == 1) ? guikit::StateColor::Danger() : guikit::StateColor::Ghost(),
            .active = (s_sonar.selectedTargetId == 1)
        })) {
            s_sonar.selectedTargetId = 1;
        }
        if (tb.ToolButton({
            .text = "Lock Target-B",
            .color = (s_sonar.selectedTargetId == 2) ? guikit::StateColor::Danger() : guikit::StateColor::Ghost(),
            .active = (s_sonar.selectedTargetId == 2)
        })) {
            s_sonar.selectedTargetId = 2;
        }
        if (tb.ToolButton({
            .text = "Lock Target-C",
            .color = (s_sonar.selectedTargetId == 3) ? guikit::StateColor::Info() : guikit::StateColor::Ghost(),
            .active = (s_sonar.selectedTargetId == 3)
        })) {
            s_sonar.selectedTargetId = 3;
        }

        tb.Spacer();

        if (tb.ToolButton({
            .text = "Reset Zoom",
            .icon = guikit::Icon::Refresh()
        })) {
            s_sonar.requestResetView = true;
        }
    }

    // Main Autonomous SonarView Widget
    s_sonar.lastResp = ui.SonarView(s_sonar.polarMatrix, {
        .numBeams = s_sonar.numBeams,
        .numRangeBins = s_sonar.numRangeBins,
        .displayMode = static_cast<guikit::SonarDisplayMode>(s_sonar.displayModeIndex),
        .scanMode = static_cast<guikit::SonarScanMode>(s_sonar.scanModeIndex),
        .colormap = s_sonar.colormap,
        .maxRangeMeters = s_sonar.maxRangeMeters,
        .headingDeg = s_sonar.headingDeg,
        .sectorApertureDeg = s_sonar.sectorApertureDeg,
        .gainDb = s_sonar.gainDb,
        .tvgAmount = s_sonar.tvgAmount,
        .clutterThreshold = s_sonar.clutterThreshold,
        .persistence = s_sonar.persistence,
        .sweepAngleDeg = s_sonar.sweepAngleDeg,
        .targets = s_sonar.targetViews,
        .guardZone = {
            .enabled = s_sonar.guardZoneEnabled,
            .minRangeMeters = s_sonar.guardMinRange,
            .maxRangeMeters = s_sonar.guardMaxRange,
            .startBearingDeg = s_sonar.guardStartBrg,
            .endBearingDeg = s_sonar.guardEndBrg
        },
        .showRangeRings = s_sonar.showRangeRings,
        .showBearingDial = s_sonar.showBearingDial,
        .showTargets = s_sonar.showTargets,
        .showTargetLabels = s_sonar.showTargetLabels,
        .showColorbar = s_sonar.showColorbar,
        .showEblVrm = s_sonar.showEblVrm,
        .showHudOverlay = s_sonar.showHudOverlay,
        .selectedTargetId = &s_sonar.selectedTargetId,
        .eblBearingDeg = &s_sonar.eblBearingDeg,
        .vrmRangeMeters = &s_sonar.vrmRangeMeters,
        .viewRangeZoom = &s_sonar.rangeZoom,
        .requestResetView = s_sonar.requestResetView,
        .requestClearPersistence = s_sonar.requestClearPersistence,
        .heightMode = guikit::SizeMode::Stretch
    });

    s_sonar.requestResetView = false;
    s_sonar.requestClearPersistence = false;
}

std::string GenerateSonarCode(const DemoContext& /*ctx*/) {
    static constexpr std::array<const char*, 3> kModeEnums = {
        "SonarDisplayMode::PPI_360",
        "SonarDisplayMode::SectorWedge",
        "SonarDisplayMode::BScan"
    };
    static constexpr std::array<const char*, 2> kScanEnums = {
        "SonarScanMode::MultibeamContinuous",
        "SonarScanMode::RotatingSweep"
    };
    const std::string_view cmapName = guikit::GetColormapName(s_sonar.colormap);

    char buf[2048];
    std::snprintf(
        buf,
        sizeof(buf),
        "// Polar Heatmap / Diver Detection Sonar (DDS) / PPI Radar Widget\n"
        "// polarMatrix: row-major [numBeams x numRangeBins] float backscatter grid\n"
        "auto resp = ui.SonarView(polarMatrix, {\n"
        "    .numBeams         = %u,\n"
        "    .numRangeBins     = %u,\n"
        "    .displayMode      = guikit::%s,\n"
        "    .scanMode         = guikit::%s,\n"
        "    .colormap         = guikit::Colormap::%.*s,\n"
        "    .maxRangeMeters   = %.1ff,\n"
        "    .sectorApertureDeg= %.1ff,\n"
        "    .gainDb           = %.1ff,\n"
        "    .tvgAmount        = %.2ff,\n"
        "    .clutterThreshold = %.2ff,\n"
        "    .persistence      = %.2ff,\n"
        "    .targets          = trackedTargets,\n"
        "    .guardZone        = {\n"
        "        .enabled          = %s,\n"
        "        .minRangeMeters   = %.1ff,\n"
        "        .maxRangeMeters   = %.1ff,\n"
        "        .startBearingDeg  = %.1ff,\n"
        "        .endBearingDeg    = %.1ff\n"
        "    },\n"
        "    .selectedTargetId = &selectedTargetId,\n"
        "    .eblBearingDeg    = &eblBearingDeg,\n"
        "    .vrmRangeMeters   = &vrmRangeMeters,\n"
        "    .heightMode       = guikit::SizeMode::Stretch\n"
        "});\n"
        "if (resp.guardZoneAlarm) {\n"
        "    // Handle perimeter intrusion (%u active diver/UUV contacts in guard sector)\n"
        "}\n",
        s_sonar.numBeams,
        s_sonar.numRangeBins,
        kModeEnums[std::clamp(s_sonar.displayModeIndex, 0, 2)],
        kScanEnums[std::clamp(s_sonar.scanModeIndex, 0, 1)],
        static_cast<int>(cmapName.size()),
        cmapName.data(),
        s_sonar.maxRangeMeters,
        s_sonar.sectorApertureDeg,
        s_sonar.gainDb,
        s_sonar.tvgAmount,
        s_sonar.clutterThreshold,
        s_sonar.persistence,
        s_sonar.guardZoneEnabled ? "true" : "false",
        s_sonar.guardMinRange,
        s_sonar.guardMaxRange,
        s_sonar.guardStartBrg,
        s_sonar.guardEndBrg,
        s_sonar.lastResp.guardZoneIntruderCount
    );
    return std::string(buf);
}

} // namespace guikit::demo
