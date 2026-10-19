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

struct WaterfallDemoState {
    bool initialized{false};
    bool liveStream{true};
    int scenarioIndex{0};       // 0 = RF SDR Spectrum, 1 = LOFAR/DEMON Harmonics, 2 = Sonar Towed-Array DOA/BTR, 3 = RF MUSIC DOA
    int modeIndex{0};           // 0 = SpectrumRtsa, 1 = DoaBtr
    int layoutIndex{0};         // 0 = SplitTraceAndWaterfall, 1 = WaterfallOnly, 2 = TraceOnly
    Colormap colormap{Colormap::Turbo};

    uint32_t numBins{512};
    uint32_t historyRows{240};
    float historyDurationSec{30.0f};
    float minX{0.0f};
    float maxX{24000.0f};
    float topTraceRatio{0.30f};
    float updateRateHz{30.0f};

    float dbFloor{-108.0f};
    float dbCeil{-6.0f};
    float gainDb{0.0f};
    float thresholdGateDb{-58.0f};
    float gateSuppression{0.38f};
    float gamma{0.95f};
    float averageAlpha{0.12f};
    float peakDecayDbPerSec{1.4f};

    bool showLiveTrace{true};
    bool showPeakHold{true};
    bool showAverageTrace{true};
    bool showThresholdGate{true};
    bool showPeakMarkers{true};
    bool showTracks{true};
    bool showTrackGates{true};
    bool showHarmonicComb{false};
    bool showColorbar{true};
    bool showHudOverlay{true};
    bool showGrid{true};
    bool invertTimeAxis{false};

    int selectedTrackId{1};
    float harmonicFundamentalX{1450.0f};
    bool requestResetZoom{false};
    bool requestResetPeakHold{false};
    bool requestClearHistory{false};

    uint64_t sliceSequence{1};
    std::vector<float> currentSliceDb{};
    std::vector<float> initialHistoryMatrix{};
    std::vector<WaterfallTrack> tracks{};

    double simTimeSec{0.0};
    double sliceAccumSec{0.0};
    std::chrono::steady_clock::time_point lastWallClock{};

    WaterfallViewResponse lastResp{};
};

WaterfallDemoState s_wf{};

[[nodiscard]] inline float FastHashNoise(uint32_t x, uint32_t y, uint32_t seed) noexcept {
    uint32_t n = x * 374761393u + y * 668265263u + seed * 1442695041u;
    n = (n ^ (n >> 13u)) * 1274126177u;
    n = n ^ (n >> 16u);
    return static_cast<float>(n & 0xFFFFu) / 65535.0f;
}

[[nodiscard]] inline float GaussianLobe(float x, float center, float sigma) noexcept {
    const float d = (x - center) / std::max(1e-4f, sigma);
    return std::exp(-0.5f * d * d);
}

// Synthesizes a single slice (1 row of `numBins` dB values) and updates track positions at simulation time `tSec`
void SynthesizeSliceAtTime(
    const WaterfallDemoState& st,
    double tSec,
    uint32_t rowSeed,
    float* outSliceDb
) noexcept {
    const uint32_t N = st.numBins;
    const float spanX = st.maxX - st.minX;

    if (st.modeIndex == 0) {
        // =================================================================
        // MODE A: SPECTRUM RTSA (Frequency Hz vs Time)
        // =================================================================
        if (st.scenarioIndex == 1) {
            // Scenario 1: Hydroacoustic LOFAR / DEMON (Submarine Turbine Harmonics + Generator Tonal)
            const float f0 = 1450.0f + 22.0f * static_cast<float>(std::sin(tSec * 0.18));
            const float genF = 7850.0f + 95.0f * static_cast<float>(std::cos(tSec * 0.27));
            const float cavBurst = std::max(0.0f, static_cast<float>(std::sin(tSec * 1.9) - 0.55f));

            for (uint32_t i = 0; i < N; ++i) {
                const float u = static_cast<float>(i) / static_cast<float>(N - 1);
                const float f = st.minX + u * spanX;
                const float n1 = FastHashNoise(i, rowSeed, 101u);
                const float n2 = FastHashNoise(i + 17u, rowSeed * 3u, 211u);

                // Ambient sea-state 1/f acoustic background noise
                float linPower = std::pow(10.0f, (-94.0f - 10.0f * std::sqrt(u) + (n1 + n2 - 1.0f) * 3.8f) / 10.0f);

                // Fundamental shaft/blade harmonic comb: f0, 2*f0, 3*f0, 4*f0, 5*f0, 6*f0
                const float harmAmps[6] = {-18.0f, -24.0f, -31.0f, -38.0f, -45.0f, -52.0f};
                for (int h = 1; h <= 6; ++h) {
                    const float hf = f0 * static_cast<float>(h);
                    const float lobe = GaussianLobe(f, hf, 26.0f + 4.0f * h);
                    if (lobe > 1e-4f) {
                        linPower += lobe * std::pow(10.0f, harmAmps[h - 1] / 10.0f);
                    }
                }

                // Auxiliary diesel-electric tonal + sidebands
                linPower += GaussianLobe(f, genF, 34.0f) * std::pow(10.0f, -22.0f / 10.0f);
                linPower += GaussianLobe(f, genF - 240.0f, 22.0f) * std::pow(10.0f, -44.0f / 10.0f);
                linPower += GaussianLobe(f, genF + 240.0f, 22.0f) * std::pow(10.0f, -44.0f / 10.0f);

                // Broadband cavitation modulation envelope
                if (cavBurst > 0.0f) {
                    const float cav = cavBurst * GaussianLobe(f, 4600.0f, 1100.0f) * (0.6f + 0.8f * n1);
                    linPower += cav * std::pow(10.0f, -42.0f / 10.0f);
                }

                outSliceDb[i] = 10.0f * std::log10(std::max(1e-13f, linPower));
            }
        } else {
            // Scenario 0: RF SDR Wideband RTSA (CW Pilot + QPSK Carrier + FHSS Hopper + LFM Radar Chirp)
            const float pilotF = 3400.0f + 320.0f * static_cast<float>(std::sin(tSec * 0.35));
            const float qpskCenter = 8600.0f;
            const float qpskBw = 2200.0f;

            // FHSS Hopper changes slot every 0.85 seconds
            const uint32_t hopSlot = static_cast<uint32_t>(std::floor(std::max(0.0, tSec + 100.0) / 0.85));
            const float hopFreq = 13200.0f + static_cast<float>((hopSlot * 2654435761u) % 5200u);

            // Swept LFM Chirp between 19.2 kHz and 22.4 kHz
            const float chirpPhase = static_cast<float>(std::fmod(std::max(0.0, tSec + 100.0) * 0.65, 1.0));
            const float chirpF = 19200.0f + chirpPhase * 3000.0f;

            for (uint32_t i = 0; i < N; ++i) {
                const float u = static_cast<float>(i) / static_cast<float>(N - 1);
                const float f = st.minX + u * spanX;
                const float n1 = FastHashNoise(i, rowSeed, 313u);
                const float n2 = FastHashNoise(i + 29u, rowSeed * 5u, 719u);

                float linPower = std::pow(10.0f, (-98.0f + (n1 + n2 - 1.0f) * 3.5f) / 10.0f);

                // 1. Drifting CW Beacon + 2nd harmonic
                linPower += GaussianLobe(f, pilotF, 65.0f) * std::pow(10.0f, -15.0f / 10.0f);
                linPower += GaussianLobe(f, pilotF * 2.0f, 75.0f) * std::pow(10.0f, -46.0f / 10.0f);

                // 2. Wideband QPSK flat-top carrier with raised-cosine roll-off
                const float dfQpsk = std::abs(f - qpskCenter);
                if (dfQpsk < qpskBw * 0.75f) {
                    const float edge = std::clamp((qpskBw * 0.65f - dfQpsk) / (qpskBw * 0.25f), 0.0f, 1.0f);
                    const float ripple = 0.75f + 0.5f * n1;
                    linPower += edge * ripple * std::pow(10.0f, -31.0f / 10.0f);
                }

                // 3. FHSS Frequency Hopper burst
                linPower += GaussianLobe(f, hopFreq, 115.0f) * std::pow(10.0f, -18.0f / 10.0f);

                // 4. Swept LFM Chirp
                linPower += GaussianLobe(f, chirpF, 130.0f) * std::pow(10.0f, -24.0f / 10.0f);

                outSliceDb[i] = 10.0f * std::log10(std::max(1e-13f, linPower));
            }
        }
    } else {
        // =================================================================
        // MODE B: DOA / BTR TRACKER (Azimuth Bearing -180..+180 deg vs Time)
        // =================================================================
        // Target 1: ALPHA-SUB drifting slowly across port quarter
        const float brg1 = -62.0f + 18.0f * static_cast<float>(std::sin(tSec * 0.075));
        // Target 2: BRAVO-SURFACE crossing target (crosses Target 3 / center boresight)
        const float brg2 = -8.0f + 36.0f * static_cast<float>(std::cos(tSec * 0.095));
        // Target 3: CHARLIE-ESCORT on starboard beam with periodic fade
        const float brg3 = 78.0f + 9.0f * static_cast<float>(std::sin(tSec * 0.14));
        const float fade3 = 0.55f + 0.45f * static_cast<float>(std::cos(tSec * 0.45));
        // Target 4: FAST-TORPEDO / AGILE THREAT high bearing rate
        const float brg4 = 128.0f - 42.0f * static_cast<float>(std::sin(tSec * 0.22));

        const bool isMusic = (st.scenarioIndex == 3);
        const float beamWidth = isMusic ? 1.8f : 4.2f;
        const float sidelobeLevel = isMusic ? 0.015f : 0.085f;

        for (uint32_t i = 0; i < N; ++i) {
            const float u = static_cast<float>(i) / static_cast<float>(N - 1);
            const float deg = st.minX + u * spanX;
            const float n1 = FastHashNoise(i, rowSeed, 509u);
            const float n2 = FastHashNoise(i + 41u, rowSeed * 7u, 883u);

            // Isotropic ambient noise + slight baffle degradation near +/-180 deg
            const float endfireLoss = 3.5f * std::pow(std::abs(deg) / 180.0f, 2.0f);
            float linPower = std::pow(10.0f, (-92.0f + endfireLoss + (n1 + n2 - 1.0f) * 3.0f) / 10.0f);

            auto addArrayResponse = [&](float targetBrg, float peakDb, float widthDeg) {
                const float d = deg - targetBrg;
                const float mainLobe = GaussianLobe(deg, targetBrg, widthDeg);
                const float side1 = sidelobeLevel * GaussianLobe(std::abs(d), widthDeg * 2.8f, widthDeg * 0.9f);
                const float resp = mainLobe + side1 * (0.6f + 0.4f * n1);
                if (resp > 1e-5f) {
                    linPower += resp * std::pow(10.0f, peakDb / 10.0f);
                }
            };

            addArrayResponse(brg1, -19.0f, beamWidth);
            addArrayResponse(brg2, -15.0f, beamWidth * 1.05f);
            addArrayResponse(brg3, -34.0f + 10.0f * fade3, beamWidth * 1.15f);
            addArrayResponse(brg4, -12.0f, beamWidth * 0.90f);

            outSliceDb[i] = 10.0f * std::log10(std::max(1e-13f, linPower));
        }
    }
}

void RebuildTracksAtTime(WaterfallDemoState& st, double nowSec) {
    st.tracks.clear();
    constexpr int kHistoryPts = 32;
    const float histDur = st.historyDurationSec;

    if (st.modeIndex == 1) {
        // 4 DOA / BTR Target Tracks
        auto buildDoaTrack = [&](int id, const char* callsign, WaterfallTrackStatus status, float levelDb, float snrDb, float gateW, auto brgFn) {
            WaterfallTrack trk{};
            trk.id = id;
            trk.callsign = callsign;
            trk.status = status;
            trk.levelDb = levelDb;
            trk.snrDb = snrDb;
            trk.gateWidth = gateW;
            trk.currentX = brgFn(nowSec);
            trk.driftRatePerSec = (brgFn(nowSec) - brgFn(nowSec - 0.5)) / 0.5f;
            trk.history.reserve(kHistoryPts);
            for (int k = 0; k < kHistoryPts; ++k) {
                const float age = (static_cast<float>(k) / static_cast<float>(kHistoryPts - 1)) * histDur;
                trk.history.push_back(WaterfallTrackPoint{
                    .timeAgoSec = age,
                    .xValue = brgFn(nowSec - static_cast<double>(age)),
                    .snrDb = snrDb
                });
            }
            st.tracks.push_back(std::move(trk));
        };

        buildDoaTrack(1, "TRK-01 ALPHA", WaterfallTrackStatus::Tracking, -19.0f, 22.5f, 9.0f, [](double t) {
            return -62.0f + 18.0f * static_cast<float>(std::sin(t * 0.075));
        });
        buildDoaTrack(2, "TRK-02 BRAVO", WaterfallTrackStatus::Tracking, -15.0f, 26.0f, 10.0f, [](double t) {
            return -8.0f + 36.0f * static_cast<float>(std::cos(t * 0.095));
        });
        buildDoaTrack(3, "TRK-03 CHARLIE", WaterfallTrackStatus::Coasting, -29.0f, 12.0f, 11.0f, [](double t) {
            return 78.0f + 9.0f * static_cast<float>(std::sin(t * 0.14));
        });
        buildDoaTrack(4, "TRK-04 TORPEDO", WaterfallTrackStatus::Alarm, -12.0f, 31.0f, 8.5f, [](double t) {
            return 128.0f - 42.0f * static_cast<float>(std::sin(t * 0.22));
        });
    } else if (st.scenarioIndex == 1) {
        // LOFAR / DEMON tracked shaft fundamental & generator tonal
        auto buildSpecTrack = [&](int id, const char* callsign, WaterfallTrackStatus status, float levelDb, float gateW, auto freqFn) {
            WaterfallTrack trk{};
            trk.id = id;
            trk.callsign = callsign;
            trk.status = status;
            trk.levelDb = levelDb;
            trk.snrDb = levelDb - (-92.0f);
            trk.gateWidth = gateW;
            trk.currentX = freqFn(nowSec);
            trk.driftRatePerSec = (freqFn(nowSec) - freqFn(nowSec - 0.5)) / 0.5f;
            trk.history.reserve(kHistoryPts);
            for (int k = 0; k < kHistoryPts; ++k) {
                const float age = (static_cast<float>(k) / static_cast<float>(kHistoryPts - 1)) * histDur;
                trk.history.push_back(WaterfallTrackPoint{
                    .timeAgoSec = age,
                    .xValue = freqFn(nowSec - static_cast<double>(age)),
                    .snrDb = trk.snrDb
                });
            }
            st.tracks.push_back(std::move(trk));
        };

        buildSpecTrack(1, "SHAFT-F0", WaterfallTrackStatus::Tracking, -18.0f, 220.0f, [](double t) {
            return 1450.0f + 22.0f * static_cast<float>(std::sin(t * 0.18));
        });
        buildSpecTrack(2, "AUX-GEN", WaterfallTrackStatus::Tracking, -22.0f, 260.0f, [](double t) {
            return 7850.0f + 95.0f * static_cast<float>(std::cos(t * 0.27));
        });
    } else {
        // RF SDR tracked drifting pilot beacon
        WaterfallTrack trk{};
        trk.id = 1;
        trk.callsign = "CW-BEACON";
        trk.status = WaterfallTrackStatus::Tracking;
        trk.levelDb = -15.0f;
        trk.snrDb = 32.0f;
        trk.gateWidth = 420.0f;
        trk.currentX = 3400.0f + 320.0f * static_cast<float>(std::sin(nowSec * 0.35));
        trk.driftRatePerSec = 320.0f * 0.35f * static_cast<float>(std::cos(nowSec * 0.35));
        for (int k = 0; k < kHistoryPts; ++k) {
            const float age = (static_cast<float>(k) / static_cast<float>(kHistoryPts - 1)) * histDur;
            const double t = nowSec - static_cast<double>(age);
            trk.history.push_back(WaterfallTrackPoint{
                .timeAgoSec = age,
                .xValue = 3400.0f + 320.0f * static_cast<float>(std::sin(t * 0.35)),
                .snrDb = 32.0f
            });
        }
        st.tracks.push_back(std::move(trk));
    }
}

void LoadWaterfallScenario(WaterfallDemoState& st, int scenarioIdx) {
    st.scenarioIndex = std::clamp(scenarioIdx, 0, 3);
    if (st.scenarioIndex == 0) {
        // RF SDR Wideband Spectrum RTSA
        st.modeIndex = 0;
        st.colormap = Colormap::Turbo;
        st.minX = 0.0f;
        st.maxX = 24000.0f;
        st.dbFloor = -108.0f;
        st.dbCeil = -6.0f;
        st.thresholdGateDb = -58.0f;
        st.showHarmonicComb = false;
        st.showTracks = true;
    } else if (st.scenarioIndex == 1) {
        // Hydroacoustic LOFAR / DEMON Harmonic Analyzer
        st.modeIndex = 0;
        st.colormap = Colormap::Inferno;
        st.minX = 0.0f;
        st.maxX = 12000.0f;
        st.dbFloor = -106.0f;
        st.dbCeil = -10.0f;
        st.thresholdGateDb = -60.0f;
        st.harmonicFundamentalX = 1450.0f;
        st.showHarmonicComb = true;
        st.showTracks = true;
    } else if (st.scenarioIndex == 2) {
        // Towed-Array Sonar DOA / BTR Tracker
        st.modeIndex = 1;
        st.colormap = Colormap::Blue;
        st.minX = -180.0f;
        st.maxX = +180.0f;
        st.dbFloor = -102.0f;
        st.dbCeil = -6.0f;
        st.thresholdGateDb = -54.0f;
        st.showHarmonicComb = false;
        st.showTracks = true;
    } else {
        // RF MUSIC Super-Resolution DOA Tracker
        st.modeIndex = 1;
        st.colormap = Colormap::Green;
        st.minX = -180.0f;
        st.maxX = +180.0f;
        st.dbFloor = -104.0f;
        st.dbCeil = -6.0f;
        st.thresholdGateDb = -52.0f;
        st.showHarmonicComb = false;
        st.showTracks = true;
    }

    st.simTimeSec = 60.0;
    st.sliceAccumSec = 0.0;
    st.currentSliceDb.resize(st.numBins);
    st.initialHistoryMatrix.resize(static_cast<size_t>(st.historyRows) * st.numBins);

    // Pre-populate all historyRows so the waterfall is 100% full immediately upon switching scenarios
    const double rowDt = static_cast<double>(st.historyDurationSec) / static_cast<double>(std::max(1u, st.historyRows - 1));
    for (uint32_t age = 0; age < st.historyRows; ++age) {
        const double tRow = st.simTimeSec - static_cast<double>(age) * rowDt;
        float* dstRow = st.initialHistoryMatrix.data() + static_cast<size_t>(age) * st.numBins;
        SynthesizeSliceAtTime(st, tRow, 1000u - age, dstRow);
    }
    std::copy_n(st.initialHistoryMatrix.data(), st.numBins, st.currentSliceDb.data());
    RebuildTracksAtTime(st, st.simTimeSec);

    st.requestClearHistory = true;
    st.requestResetZoom = true;
    st.sliceSequence++;
    st.lastWallClock = std::chrono::steady_clock::now();
    st.initialized = true;
}

void StepWaterfallSimulation(WaterfallDemoState& st) {
    if (!st.initialized) {
        LoadWaterfallScenario(st, 0);
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const double dt = std::clamp(std::chrono::duration<double>(now - st.lastWallClock).count(), 0.0, 0.1);
    st.lastWallClock = now;

    if (!st.liveStream) {
        return;
    }

    st.simTimeSec += dt;
    st.sliceAccumSec += dt;

    const double slicePeriod = 1.0 / std::clamp(static_cast<double>(st.updateRateHz), 5.0, 60.0);
    if (st.sliceAccumSec >= slicePeriod) {
        st.sliceAccumSec = std::fmod(st.sliceAccumSec, slicePeriod);
        st.sliceSequence++;
        SynthesizeSliceAtTime(st, st.simTimeSec, static_cast<uint32_t>(st.sliceSequence), st.currentSliceDb.data());
        RebuildTracksAtTime(st, st.simTimeSec);
    }
}

} // namespace

void RenderWaterfallInspector(guikit::Context& ui, DemoContext& /*ctx*/) {
    if (!s_wf.initialized) {
        LoadWaterfallScenario(s_wf, 0);
    }

    static constexpr std::array<std::string_view, 4> kScenarios = {
        "RTSA: RF SDR Band (FHSS + QPSK)",
        "RTSA: Acoustic LOFAR / DEMON",
        "DOA/BTR: Towed-Array Sonar",
        "DOA/BTR: RF MUSIC Direction Finder"
    };
    int scenIdx = s_wf.scenarioIndex;
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Preset Scenario",
        .layout = guikit::ElementLayout::Top
    })) {
        if (ui.Size(guikit::UiSize::Sm).Select(scenIdx, kScenarios).changed) {
            LoadWaterfallScenario(s_wf, scenIdx);
        }
    }

    static constexpr std::array<std::string_view, 3> kLayouts = {
        "Split (Trace + Waterfall)",
        "Waterfall Only",
        "Live Trace Only"
    };
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Viewport Layout",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Select(s_wf.layoutIndex, kLayouts);
    }

    int cmapIdx = static_cast<int>(s_wf.colormap);
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "GPU Colormap LUT",
        .layout = guikit::ElementLayout::Top
    })) {
        if (ui.Size(guikit::UiSize::Sm).Select(cmapIdx, guikit::GetAllColormapNames()).changed) {
            s_wf.colormap = static_cast<guikit::Colormap>(cmapIdx);
        }
    }

    ui.Size(guikit::UiSize::Xs).Heading({.label = "GPU Ring-Buffer & Conditioning", .divider = true});

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Digital Gain",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Slider(s_wf.gainDb, -20.0f, 20.0f, {
            .valueFormat = "%+.1f dB"
        });
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Dynamic Range Floor",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Slider(s_wf.dbFloor, -130.0f, -45.0f, {
            .valueFormat = "%.0f dB"
        });
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Dynamic Range Ceiling",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Slider(s_wf.dbCeil, -40.0f, 10.0f, {
            .valueFormat = "%.0f dB"
        });
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Detection Gate",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Slider(s_wf.thresholdGateDb, s_wf.dbFloor + 5.0f, s_wf.dbCeil - 5.0f, {
            .valueFormat = "%+.0f dB"
        });
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Noise Gate Suppression",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Slider(s_wf.gateSuppression, 0.0f, 1.0f, {
            .valueFormat = "%.2f"
        });
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Contrast Gamma",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Slider(s_wf.gamma, 0.45f, 2.0f, {
            .valueFormat = "%.2f"
        });
    }

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({
        .label = "Scroll Rate (Slices/sec)",
        .layout = guikit::ElementLayout::Top
    })) {
        ui.Size(guikit::UiSize::Sm).Slider(s_wf.updateRateHz, 5.0f, 60.0f, {
            .valueFormat = "%.0f Hz"
        });
    }

    ui.Size(guikit::UiSize::Xs).Heading({.label = "Traces & Tracking Overlays", .divider = true});

    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "Live Instantaneous Trace"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_wf.showLiveTrace);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "Peak Hold Envelope"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_wf.showPeakHold);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "RMS Exponential Average"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_wf.showAverageTrace);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "Auto Peak / DOA Callouts"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_wf.showPeakMarkers);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "DOA / Emitter Target Tracks"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_wf.showTracks);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "Track Association Gates"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_wf.showTrackGates);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "Harmonic Comb (f0..6f0)"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_wf.showHarmonicComb);
    }
    if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = "Invert Time Flow (Bottom-Up)"})) {
        ui.Size(guikit::UiSize::Sm).Checkbox(s_wf.invertTimeAxis);
    }
}

void RenderWaterfallStage(guikit::Context& ui, DemoContext& /*ctx*/) {
    StepWaterfallSimulation(s_wf);
    if (s_wf.liveStream) {
        ui.RequestRepaint(2);
    }

    // Top Operator Console Toolbar
    if (auto tb = ui.Size(guikit::UiSize::Sm).Toolbar()) {
        if (tb.ToolButton({
            .label = s_wf.liveStream ? "STREAMING" : "FROZEN",
            .icon = s_wf.liveStream ? guikit::Icon::Pause() : guikit::Icon::Play(),
            .variant = s_wf.liveStream ? guikit::UiVariant::Success : guikit::UiVariant::Warning,
            .active = s_wf.liveStream
        })) {
            s_wf.liveStream = !s_wf.liveStream;
        }

        tb.Separator();

        if (tb.ToolButton({
            .label = "Spectrum RTSA",
            .variant = (s_wf.modeIndex == 0 && s_wf.scenarioIndex == 0) ? guikit::UiVariant::Primary : guikit::UiVariant::Ghost,
            .active = (s_wf.modeIndex == 0 && s_wf.scenarioIndex == 0)
        })) {
            LoadWaterfallScenario(s_wf, 0);
        }
        if (tb.ToolButton({
            .label = "LOFAR Harmonics",
            .variant = (s_wf.scenarioIndex == 1) ? guikit::UiVariant::Primary : guikit::UiVariant::Ghost,
            .active = (s_wf.scenarioIndex == 1)
        })) {
            LoadWaterfallScenario(s_wf, 1);
        }
        if (tb.ToolButton({
            .label = "Sonar DOA / BTR",
            .variant = (s_wf.scenarioIndex == 2) ? guikit::UiVariant::Info : guikit::UiVariant::Ghost,
            .active = (s_wf.scenarioIndex == 2)
        })) {
            LoadWaterfallScenario(s_wf, 2);
        }
        if (tb.ToolButton({
            .label = "MUSIC DF",
            .variant = (s_wf.scenarioIndex == 3) ? guikit::UiVariant::Info : guikit::UiVariant::Ghost,
            .active = (s_wf.scenarioIndex == 3)
        })) {
            LoadWaterfallScenario(s_wf, 3);
        }

        tb.Separator();

        if (tb.ToolButton({
            .label = "Split",
            .variant = (s_wf.layoutIndex == 0) ? guikit::UiVariant::Secondary : guikit::UiVariant::Ghost,
            .active = (s_wf.layoutIndex == 0)
        })) {
            s_wf.layoutIndex = 0;
        }
        if (tb.ToolButton({
            .label = "Waterfall",
            .variant = (s_wf.layoutIndex == 1) ? guikit::UiVariant::Secondary : guikit::UiVariant::Ghost,
            .active = (s_wf.layoutIndex == 1)
        })) {
            s_wf.layoutIndex = 1;
        }
        if (tb.ToolButton({
            .label = "Trace",
            .variant = (s_wf.layoutIndex == 2) ? guikit::UiVariant::Secondary : guikit::UiVariant::Ghost,
            .active = (s_wf.layoutIndex == 2)
        })) {
            s_wf.layoutIndex = 2;
        }

        tb.Spacer();

        if (tb.ToolButton({
            .label = "Clear Peak",
            .icon = guikit::Icon::Refresh()
        })) {
            s_wf.requestResetPeakHold = true;
        }
        if (tb.ToolButton({
            .label = "Reset Zoom",
            .icon = guikit::Icon::Search()
        })) {
            s_wf.requestResetZoom = true;
        }
    }

    // Main Autonomous Hardware-Accelerated WaterfallView Widget
    s_wf.lastResp = ui.WaterfallView(s_wf.currentSliceDb, {
        .mode = static_cast<guikit::WaterfallMode>(s_wf.modeIndex),
        .viewLayout = static_cast<guikit::WaterfallLayout>(s_wf.layoutIndex),
        .colormap = s_wf.colormap,
        .numBins = s_wf.numBins,
        .historyRows = s_wf.historyRows,
        .historyDurationSec = s_wf.historyDurationSec,
        .minX = s_wf.minX,
        .maxX = s_wf.maxX,
        .topTraceHeightRatio = s_wf.topTraceRatio,
        .invertTimeAxis = s_wf.invertTimeAxis,
        .sliceSequence = s_wf.sliceSequence,
        .initialHistoryMatrix = s_wf.initialHistoryMatrix,
        .dbFloor = s_wf.dbFloor,
        .dbCeil = s_wf.dbCeil,
        .gainDb = s_wf.gainDb,
        .thresholdGateDb = s_wf.thresholdGateDb,
        .gateSuppression = s_wf.gateSuppression,
        .gamma = s_wf.gamma,
        .averageAlpha = s_wf.averageAlpha,
        .peakDecayDbPerSec = s_wf.peakDecayDbPerSec,
        .tracks = s_wf.tracks,
        .showLiveTrace = s_wf.showLiveTrace,
        .showPeakHold = s_wf.showPeakHold,
        .showAverageTrace = s_wf.showAverageTrace,
        .showThresholdGate = s_wf.showThresholdGate,
        .showPeakMarkers = s_wf.showPeakMarkers,
        .showTracks = s_wf.showTracks,
        .showTrackGates = s_wf.showTrackGates,
        .showHarmonicComb = s_wf.showHarmonicComb,
        .showColorbar = s_wf.showColorbar,
        .showHudOverlay = s_wf.showHudOverlay,
        .showGrid = s_wf.showGrid,
        .selectedTrackId = &s_wf.selectedTrackId,
        .harmonicFundamentalX = &s_wf.harmonicFundamentalX,
        .thresholdGateDbPtr = &s_wf.thresholdGateDb,
        .requestResetZoom = s_wf.requestResetZoom,
        .requestResetPeakHold = s_wf.requestResetPeakHold,
        .requestClearHistory = s_wf.requestClearHistory,
        .heightMode = guikit::SizeMode::Stretch
    });

    s_wf.requestResetZoom = false;
    s_wf.requestResetPeakHold = false;
    s_wf.requestClearHistory = false;
}

std::string GenerateWaterfallCode(const DemoContext& /*ctx*/) {
    static constexpr std::array<const char*, 2> kModeEnums = {
        "WaterfallMode::SpectrumRtsa",
        "WaterfallMode::DoaBtr"
    };
    static constexpr std::array<const char*, 3> kLayoutEnums = {
        "WaterfallLayout::SplitTraceAndWaterfall",
        "WaterfallLayout::WaterfallOnly",
        "WaterfallLayout::TraceOnly"
    };
    const std::string_view cmapName = guikit::GetColormapName(s_wf.colormap);

    char buf[2048];
    std::snprintf(
        buf,
        sizeof(buf),
        "// Hardware Ring-Buffer Waterfall (RTSA Spectrum Analyzer & DOA / BTR Target Tracker)\n"
        "// Uploads only 1 row (glTexSubImage2D) per new slice; 0%% CPU scrolling via GLSL modulo ring-buffer\n"
        "auto resp = ui.WaterfallView(latestSliceDb, {\n"
        "    .mode               = guikit::%s,\n"
        "    .viewLayout         = guikit::%s,\n"
        "    .colormap           = guikit::Colormap::%.*s,\n"
        "    .numBins            = %u,\n"
        "    .historyRows        = %u,\n"
        "    .historyDurationSec = %.1ff,\n"
        "    .minX               = %.1ff,\n"
        "    .maxX               = %.1ff,\n"
        "    .sliceSequence      = sliceSeqCounter,\n"
        "    .dbFloor            = %.1ff,\n"
        "    .dbCeil             = %.1ff,\n"
        "    .gainDb             = %+.1ff,\n"
        "    .thresholdGateDb    = %.1ff,\n"
        "    .gateSuppression    = %.2ff,\n"
        "    .gamma              = %.2ff,\n"
        "    .tracks             = activeTracks,\n"
        "    .showLiveTrace      = %s,\n"
        "    .showPeakHold       = %s,\n"
        "    .showAverageTrace   = %s,\n"
        "    .showTracks         = %s,\n"
        "    .showHarmonicComb   = %s,\n"
        "    .selectedTrackId    = &selectedTrackId,\n"
        "    .thresholdGateDbPtr = &thresholdGateDb,\n"
        "    .heightMode         = guikit::SizeMode::Stretch\n"
        "});\n",
        kModeEnums[std::clamp(s_wf.modeIndex, 0, 1)],
        kLayoutEnums[std::clamp(s_wf.layoutIndex, 0, 2)],
        static_cast<int>(cmapName.size()),
        cmapName.data(),
        s_wf.numBins,
        s_wf.historyRows,
        s_wf.historyDurationSec,
        s_wf.minX,
        s_wf.maxX,
        s_wf.dbFloor,
        s_wf.dbCeil,
        s_wf.gainDb,
        s_wf.thresholdGateDb,
        s_wf.gateSuppression,
        s_wf.gamma,
        s_wf.showLiveTrace ? "true" : "false",
        s_wf.showPeakHold ? "true" : "false",
        s_wf.showAverageTrace ? "true" : "false",
        s_wf.showTracks ? "true" : "false",
        s_wf.showHarmonicComb ? "true" : "false"
    );
    return std::string(buf);
}

} // namespace guikit::demo
