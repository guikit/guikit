#include "../DemoContext.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

namespace guikit::demo {

namespace {

constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;

struct SignalDemoState {
    size_t presetIdx = 0;
    float sampleRate = 48000.0f;
    std::vector<float> samples;
    std::vector<guikit::SignalMarker> markers;
    guikit::SignalSelection selection{};
    double playheadSec = 0.42;
    bool livePlayhead = false;
    std::chrono::steady_clock::time_point lastWallClock{};

    guikit::SignalPanelOrder panelOrder = guikit::SignalPanelOrder::WaveformTop;
    float splitRatio = 0.45f;
    guikit::AmplitudeScale amplitudeScale = guikit::AmplitudeScale::dBFS;
    guikit::FrequencyScale frequencyScale = guikit::FrequencyScale::Linear;
    guikit::SpectrogramColormap colormap = guikit::SpectrogramColormap::Inferno;
    guikit::FftWindowType fftWindow = guikit::FftWindowType::Hann;
    size_t fftSizeIdx = 1; // 0=256, 1=512, 2=1024, 3=2048
    float dbFloor = -92.0f;
    float waveformGain = 1.0f;

    bool showOverviewBar = true;
    bool showTimeRuler = true;
    bool showAmplitudeRuler = true;
    bool showFrequencyRuler = true;
    bool showColorbar = true;
    bool showGrid = true;
    bool showRms = true;
    bool showCrosshair = true;

    bool requestResetView = false;
    std::optional<double> requestViewStartSec{};
    std::optional<double> requestViewEndSec{};
    std::optional<guikit::Vec2> requestFreqRangeHz{};

    guikit::SignalViewResponse lastResp{};
    bool initialized = false;

    static constexpr std::array<std::string_view, 4> presetNames = {
        "1. Harmonic Speech & Formant Chirp (96k smp / 2.0s)",
        "2. LFM Radar & Hopping Telemetry (96k smp / 2.0s)",
        "3. Percussive Transients & Overdrive (96k smp / 2.0s)",
        "4. Ultra-Long Telemetry Stream (1.2M smp / 25.0s)"
    };

    static constexpr std::array<std::string_view, 2> panelOrderNames = {
        "Waveform Top", "Spectrogram Top"
    };

    static constexpr std::array<std::string_view, 2> ampScaleNames = {
        "dBFS", "Linear"
    };

    static constexpr std::array<std::string_view, 2> freqScaleNames = {
        "Linear Hz", "Log Hz"
    };

    static constexpr std::array<std::string_view, 5> colormapNames = {
        "Inferno", "Magma", "Viridis", "CyanHot", "Gray"
    };

    static constexpr std::array<std::string_view, 4> windowNames = {
        "Hann", "Hamming", "Blackman", "Rect"
    };

    static constexpr std::array<std::string_view, 4> fftSizeNames = {
        "256", "512", "1024", "2048"
    };

    static constexpr std::array<uint32_t, 4> fftSizeValues = {
        256, 512, 1024, 2048
    };
};

SignalDemoState s_sig;

uint32_t NextRng(uint32_t& state) noexcept {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

float WhiteNoise(uint32_t& state) noexcept {
    return (static_cast<float>(NextRng(state) & 0x00FFFFFFu) / 8388608.0f) - 1.0f;
}

void BuildSignalPreset(SignalDemoState& st, size_t preset) {
    st.presetIdx = preset;
    st.sampleRate = 48000.0f;
    st.markers.clear();
    uint32_t rng = 0x9E3779B9u + static_cast<uint32_t>(preset * 101u);

    if (preset == 0) {
        // 2.0 seconds @ 48 kHz = 96,000 samples:
        // Segment A (0.0 - 0.55s): Voiced harmonic series with formant sweep
        // Segment B (0.55 - 0.85s): High-frequency sibilant burst + FM bell
        // Segment C (0.85 - 1.45s): Multi-harmonic exponential & linear chirp
        // Segment D (1.45 - 2.00s): Vibrato chord + decaying echoes
        const size_t N = 96000;
        st.samples.assign(N, 0.0f);
        double phaseF0 = 0.0;
        double phaseChirp = 0.0;
        for (size_t i = 0; i < N; ++i) {
            const float t = static_cast<float>(i) / st.sampleRate;
            float v = 0.002f * WhiteNoise(rng);

            if (t < 0.55f) {
                const float env = std::sin(std::numbers::pi_v<float> * (t / 0.55f));
                const float f0 = 220.0f + 45.0f * std::sin(kTwoPi * 3.2f * t);
                phaseF0 += static_cast<double>(kTwoPi * f0 / st.sampleRate);
                const float f1Center = 900.0f + 700.0f * (t / 0.55f);
                const float f2Center = 2400.0f + 1600.0f * std::sin(std::numbers::pi_v<float> * (t / 0.55f));
                float sum = 0.0f;
                for (int h = 1; h <= 28; ++h) {
                    const float fh = f0 * static_cast<float>(h);
                    if (fh >= st.sampleRate * 0.48f) break;
                    const float d1 = (fh - f1Center) / 380.0f;
                    const float d2 = (fh - f2Center) / 520.0f;
                    const float formant = 0.75f * std::exp(-0.5f * d1 * d1) +
                                          0.65f * std::exp(-0.5f * d2 * d2) +
                                          0.18f / static_cast<float>(h);
                    sum += formant * std::sin(static_cast<float>(phaseF0 * h));
                }
                v += 0.48f * env * sum;
            } else if (t < 0.85f) {
                const float localT = (t - 0.55f) / 0.30f;
                const float env = std::sin(std::numbers::pi_v<float> * localT);
                const float carrier = std::sin(kTwoPi * 6800.0f * t + 4.5f * std::sin(kTwoPi * 1100.0f * t));
                const float sub = std::sin(kTwoPi * 1250.0f * t) * std::exp(-localT * 4.0f);
                v += env * (0.35f * WhiteNoise(rng) * std::sin(kTwoPi * 9500.0f * t) + 0.45f * carrier + 0.30f * sub);
            } else if (t < 1.48f) {
                const float localT = (t - 0.85f) / 0.63f;
                const float env = std::sin(std::numbers::pi_v<float> * localT);
                const float instFreq = 350.0f + 9200.0f * localT * localT;
                phaseChirp += static_cast<double>(kTwoPi * instFreq / st.sampleRate);
                v += env * (0.58f * std::sin(static_cast<float>(phaseChirp)) +
                            0.26f * std::sin(static_cast<float>(phaseChirp * 2.0)) +
                            0.12f * std::sin(static_cast<float>(phaseChirp * 3.0)));
            } else {
                const float localT = (t - 1.48f) / 0.52f;
                const float env = std::exp(-localT * 3.6f) * std::min(1.0f, localT * 25.0f);
                const float vib = 1.0f + 0.015f * std::sin(kTwoPi * 6.5f * t);
                const float c1 = std::sin(kTwoPi * 523.25f * vib * t);
                const float c2 = std::sin(kTwoPi * 659.25f * vib * t);
                const float c3 = std::sin(kTwoPi * 783.99f * vib * t);
                const float c4 = std::sin(kTwoPi * 1567.98f * vib * t);
                const float c5 = std::sin(kTwoPi * 3135.96f * vib * t);
                v += 0.85f * env * (0.32f * c1 + 0.25f * c2 + 0.20f * c3 + 0.14f * c4 + 0.09f * c5);
            }
            st.samples[i] = std::clamp(v, -0.98f, 0.98f);
        }

        st.markers = {
            {0.04, "Formant Onset", guikit::Color{56, 189, 248, 255}},
            {0.55, "FM Sibilant",   guikit::Color{245, 158, 11, 255}},
            {0.85, "Harmonic Chirp",guikit::Color{168, 85, 247, 255}},
            {1.48, "Vibrato Chord", guikit::Color{34, 197, 94, 255}}
        };
        st.selection = {
            .active = true,
            .startTimeSec = 0.88,
            .endTimeSec = 1.38,
            .hasSpectralBox = true,
            .lowFreqHz = 1200.0f,
            .highFreqHz = 14800.0f
        };
        st.playheadSec = 0.42;
    } else if (preset == 1) {
        // Preset 1: LFM Radar & Frequency-Hopping Telemetry (96,000 samples / 2.0s)
        const size_t N = 96000;
        st.samples.assign(N, 0.0f);
        double phase = 0.0;
        static constexpr std::array<float, 8> hopFreqs = {
            1800.0f, 6400.0f, 3600.0f, 11200.0f, 4800.0f, 15400.0f, 8200.0f, 19200.0f
        };
        for (size_t i = 0; i < N; ++i) {
            const float t = static_cast<float>(i) / st.sampleRate;
            float v = 0.008f * WhiteNoise(rng);
            // Continuous pilot beacon at 1.0 kHz
            v += 0.07f * std::sin(kTwoPi * 1000.0f * t);

            if (t >= 0.06f && t < 0.46f) {
                // Up-Chirp 1.5 kHz -> 20 kHz
                const float u = (t - 0.06f) / 0.40f;
                const float win = std::sin(std::numbers::pi_v<float> * u);
                const float f = 1500.0f + 18500.0f * u;
                phase += static_cast<double>(kTwoPi * f / st.sampleRate);
                v += 0.78f * win * std::sin(static_cast<float>(phase));
            } else if (t >= 0.54f && t < 0.94f) {
                // Down-Chirp 21 kHz -> 2 kHz + Simultaneous Counter-Chirp
                const float u = (t - 0.54f) / 0.40f;
                const float win = std::sin(std::numbers::pi_v<float> * u);
                const float fDown = 21000.0f - 19000.0f * u;
                const float fUp = 2000.0f + 14000.0f * u;
                phase += static_cast<double>(kTwoPi * fDown / st.sampleRate);
                v += win * (0.52f * std::sin(static_cast<float>(phase)) +
                            0.34f * std::sin(kTwoPi * fUp * t));
            } else if (t >= 1.02f && t < 1.94f) {
                // Stepped FHSS Telemetry Hops
                const float hopDur = (1.94f - 1.02f) / static_cast<float>(hopFreqs.size());
                const size_t hopIdx = std::min(hopFreqs.size() - 1, static_cast<size_t>((t - 1.02f) / hopDur));
                const float hopLocal = std::fmod(t - 1.02f, hopDur) / hopDur;
                const float env = std::sin(std::numbers::pi_v<float> * hopLocal);
                const float f = hopFreqs[hopIdx];
                v += 0.72f * env * (std::sin(kTwoPi * f * t) + 0.25f * std::sin(kTwoPi * (f + 950.0f) * t));
            }
            st.samples[i] = std::clamp(v, -0.98f, 0.98f);
        }

        st.markers = {
            {0.06, "LFM Up-Chirp",  guikit::Color{56, 189, 248, 255}},
            {0.54, "Dual X-Chirp",  guikit::Color{245, 158, 11, 255}},
            {1.02, "FHSS Hopping",  guikit::Color{34, 197, 94, 255}}
        };
        st.selection = {
            .active = true,
            .startTimeSec = 0.54,
            .endTimeSec = 0.94,
            .hasSpectralBox = true,
            .lowFreqHz = 2000.0f,
            .highFreqHz = 21000.0f
        };
        st.playheadSec = 0.74;
    } else if (preset == 2) {
        // Preset 2: Percussive Transients & Overdrive Saturation (96,000 samples / 2.0s)
        const size_t N = 96000;
        st.samples.assign(N, 0.0f);
        static constexpr std::array<float, 6> hitTimes = {0.08f, 0.38f, 0.68f, 0.98f, 1.28f, 1.58f};
        for (size_t i = 0; i < N; ++i) {
            const float t = static_cast<float>(i) / st.sampleRate;
            float v = 0.003f * WhiteNoise(rng);
            for (size_t h = 0; h < hitTimes.size(); ++h) {
                const float dt = t - hitTimes[h];
                if (dt >= 0.0f && dt < 0.28f) {
                    const float pitchDrop = 140.0f + 580.0f * std::exp(-dt * 42.0f);
                    const float bodyEnv = std::exp(-dt * 14.0f);
                    const float clickEnv = std::exp(-dt * 140.0f);
                    const float body = std::sin(kTwoPi * pitchDrop * dt);
                    const float snare = (h % 2 == 1) ? (WhiteNoise(rng) * std::exp(-dt * 28.0f) * 0.65f) : 0.0f;
                    const float metallic = std::sin(kTwoPi * (3400.0f + 900.0f * static_cast<float>(h)) * dt) *
                                           std::sin(kTwoPi * 5120.0f * dt) * std::exp(-dt * 22.0f);
                    float raw = 0.85f * bodyEnv * body + 0.55f * clickEnv * WhiteNoise(rng) + snare + 0.35f * metallic;
                    // Intentional warm soft-clipping on 4th & 6th hits
                    if (h == 3 || h == 5) {
                        raw = std::tanh(raw * 2.8f);
                    }
                    v += raw;
                }
            }
            st.samples[i] = std::clamp(v, -0.99f, 0.99f);
        }

        st.markers = {
            {0.08, "Kick Transient", guikit::Color{56, 189, 248, 255}},
            {0.38, "Snare Burst",    guikit::Color{245, 158, 11, 255}},
            {0.98, "Clipped Hit",    guikit::Color{239, 68, 68, 255}}
        };
        st.selection = {
            .active = true,
            .startTimeSec = 0.36,
            .endTimeSec = 0.60,
            .hasSpectralBox = true,
            .lowFreqHz = 800.0f,
            .highFreqHz = 18000.0f
        };
        st.playheadSec = 0.38;
    } else {
        // Preset 3: Ultra-Long 1,200,000 samples (25.0 seconds @ 48 kHz)
        // Demonstrates O(W) multi-resolution Mipmap Pyramid + on-demand STFT
        const size_t N = 1200000;
        st.samples.assign(N, 0.0f);
        double phase = 0.0;
        for (size_t i = 0; i < N; ++i) {
            const float t = static_cast<float>(i) / st.sampleRate;
            const float macroEnv = 0.55f + 0.40f * std::sin(kTwoPi * 0.18f * t);
            const float sweepFreq = 400.0f + 8500.0f * (0.5f + 0.5f * std::sin(kTwoPi * 0.12f * t));
            phase += static_cast<double>(kTwoPi * sweepFreq / st.sampleRate);

            const float pulseMod = std::max(0.0f, std::sin(kTwoPi * 1.5f * t));
            const float harmonic1 = std::sin(static_cast<float>(phase));
            const float harmonic2 = 0.45f * std::sin(static_cast<float>(phase * 2.0));
            const float carrierHi = 0.28f * pulseMod * std::sin(kTwoPi * (12000.0f + 4000.0f * std::cos(kTwoPi * 0.4f * t)) * t);
            const float noise = 0.015f * WhiteNoise(rng);

            const float v = macroEnv * (0.55f * harmonic1 + harmonic2) + carrierHi + noise;
            st.samples[i] = std::clamp(v, -0.98f, 0.98f);
        }

        st.markers = {
            {2.50,  "Sweep Crest #1", guikit::Color{56, 189, 248, 255}},
            {8.33,  "Mid Telemetry",  guikit::Color{245, 158, 11, 255}},
            {16.60, "Burst Window",   guikit::Color{168, 85, 247, 255}},
            {22.00, "Tail Marker",    guikit::Color{34, 197, 94, 255}}
        };
        st.selection = {
            .active = true,
            .startTimeSec = 6.0,
            .endTimeSec = 11.5,
            .hasSpectralBox = true,
            .lowFreqHz = 1500.0f,
            .highFreqHz = 16500.0f
        };
        st.playheadSec = 8.33;
    }
}

void EnsureSignalInitialized() {
    if (!s_sig.initialized) {
        s_sig.initialized = true;
        s_sig.lastWallClock = std::chrono::steady_clock::now();
        BuildSignalPreset(s_sig, 0);
    }
}

void StepSignalPlayback(SignalDemoState& st) {
    const auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - st.lastWallClock).count();
    st.lastWallClock = now;
    dt = std::clamp(dt, 0.0, 0.08);
    if (!st.livePlayhead || st.samples.empty()) {
        return;
    }
    if (dt < 0.004) {
        dt = 0.016;
    }
    const double totalDur = static_cast<double>(st.samples.size()) / static_cast<double>(std::max(1.0f, st.sampleRate));
    const double vStart = (st.lastResp.viewEndSec > st.lastResp.viewStartSec) ? st.lastResp.viewStartSec : 0.0;
    const double vEnd   = (st.lastResp.viewEndSec > st.lastResp.viewStartSec) ? st.lastResp.viewEndSec : totalDur;
    const double span   = std::max(1e-4, vEnd - vStart);
    st.playheadSec += dt;
    if (st.playheadSec > vEnd || st.playheadSec < vStart) {
        st.playheadSec = vStart + std::fmod(std::max(0.0, st.playheadSec - vStart), span);
    }
}

} // namespace

void RenderSignalInspector(guikit::Context& ui, DemoContext&) {
    EnsureSignalInitialized();

    ui.Switch(s_sig.livePlayhead, {
        .label = "Live Playhead Sweep",
        .description = "Animate playhead at 60 Hz across active viewport"
    });

    if (ui.Select(s_sig.presetIdx, SignalDemoState::presetNames, {
        .label = "Test Signal Generator",
        .prefix = guikit::Icon::Play()
    }).changed) {
        BuildSignalPreset(s_sig, s_sig.presetIdx);
        s_sig.requestResetView = true;
    }

    size_t orderIdx = static_cast<size_t>(s_sig.panelOrder);
    if (ui.StepSlider(orderIdx, SignalDemoState::panelOrderNames, {
        .label = "Panel Stack Order",
        .showTicks = true,
        .showLabels = true
    }).changed) {
        s_sig.panelOrder = static_cast<guikit::SignalPanelOrder>(orderIdx);
    }

    float splitPct = s_sig.splitRatio * 100.0f;
    if (ui.Slider(splitPct, 0.0f, 100.0f, {
        .label = "Top / Bottom Split Ratio",
        .valueFormat = "%.0f %%",
        .step = 1.0f
    }).changed) {
        s_sig.splitRatio = splitPct / 100.0f;
    }

    size_t ampIdx = static_cast<size_t>(s_sig.amplitudeScale);
    if (ui.StepSlider(ampIdx, SignalDemoState::ampScaleNames, {
        .label = "Waveform Amplitude Scale",
        .showTicks = true,
        .showLabels = true
    }).changed) {
        s_sig.amplitudeScale = static_cast<guikit::AmplitudeScale>(ampIdx);
    }

    size_t freqIdx = static_cast<size_t>(s_sig.frequencyScale);
    if (ui.StepSlider(freqIdx, SignalDemoState::freqScaleNames, {
        .label = "Spectrogram Frequency Scale",
        .showTicks = true,
        .showLabels = true
    }).changed) {
        s_sig.frequencyScale = static_cast<guikit::FrequencyScale>(freqIdx);
    }

    size_t cmapIdx = static_cast<size_t>(s_sig.colormap);
    if (ui.StepSlider(cmapIdx, SignalDemoState::colormapNames, {
        .label = "Spectrogram Colormap",
        .showTicks = true,
        .showLabels = true
    }).changed) {
        s_sig.colormap = static_cast<guikit::SpectrogramColormap>(cmapIdx);
    }

    ui.StepSlider(s_sig.fftSizeIdx, SignalDemoState::fftSizeNames, {
        .label = "STFT Window Size (Bins)",
        .showTicks = true,
        .showLabels = true
    });

    size_t winIdx = static_cast<size_t>(s_sig.fftWindow);
    if (ui.StepSlider(winIdx, SignalDemoState::windowNames, {
        .label = "FFT Window Function",
        .showTicks = true,
        .showLabels = true
    }).changed) {
        s_sig.fftWindow = static_cast<guikit::FftWindowType>(winIdx);
    }

    ui.Slider(s_sig.dbFloor, -120.0f, -36.0f, {
        .label = "Spectrogram Noise Floor",
        .valueFormat = "%.0f dBFS",
        .step = 2.0f
    });

    ui.Slider(s_sig.waveformGain, 0.25f, 8.0f, {
        .label = "Waveform Vertical Gain",
        .valueFormat = "%.2fx",
        .step = 0.05f
    });

    ui.Switch(s_sig.showOverviewBar, {.label = "Overview Minimap Navigator"});
    ui.Switch(s_sig.showTimeRuler, {.label = "Adaptive Time & Cue Ruler"});
    ui.Switch(s_sig.showAmplitudeRuler, {.label = "Waveform Amplitude Ruler"});
    ui.Switch(s_sig.showFrequencyRuler, {.label = "Spectrogram Frequency Ruler"});
    ui.Switch(s_sig.showColorbar, {.label = "Right dBFS Heatmap Legend"});
    ui.Switch(s_sig.showRms, {.label = "Dual Peak + RMS Envelope"});
    ui.Switch(s_sig.showGrid, {.label = "Synchronized Overlay Grid"});
    ui.Switch(s_sig.showCrosshair, {.label = "Crosshair & Live Readout HUD"});

    if (auto btnRow = ui.Row({.gap = 8.0f})) {
        if (ui.Button({
            .text = "Zoom Selection",
            .icon = guikit::Icon::Search(),
            .variant = guikit::UiVariant::Primary,
            .stretch = guikit::StretchMode::Fill,
            .disabled = !s_sig.selection.active || s_sig.selection.DurationSec() <= 1e-5,
            .size = guikit::UiSize::Sm
        })) {
            s_sig.requestViewStartSec = s_sig.selection.MinTimeSec();
            s_sig.requestViewEndSec = s_sig.selection.MaxTimeSec();
            if (s_sig.selection.hasSpectralBox && s_sig.selection.BandwidthHz() >= 50.0f) {
                s_sig.requestFreqRangeHz = guikit::Vec2{s_sig.selection.MinFreqHz(), s_sig.selection.MaxFreqHz()};
            }
        }
        if (ui.Button({
            .text = "Reset View",
            .icon = guikit::Icon::Refresh(),
            .variant = guikit::UiVariant::Secondary,
            .stretch = guikit::StretchMode::Fill,
            .size = guikit::UiSize::Sm
        })) {
            s_sig.requestResetView = true;
            s_sig.waveformGain = 1.0f;
        }
    }

    if (auto telCard = ui.Card({
        .gap = 4.0f,
        .padding = guikit::Padding::All(8.0f),
        .borderRadius = 6.0f
    })) {
        ui.Text(
            ui.Format("Signal: {:L} smp ({:.2f} s @ {:.0f} Hz)",
                      s_sig.lastResp.totalSamples,
                      s_sig.lastResp.totalDurationSec,
                      s_sig.sampleRate),
            {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Primary}
        );
        ui.Text(
            ui.Format("Viewport: [{:.4f} s .. {:.4f} s] | [{:.0f} .. {:.0f} Hz]",
                      s_sig.lastResp.viewStartSec,
                      s_sig.lastResp.viewEndSec,
                      s_sig.lastResp.viewMinFreqHz,
                      s_sig.lastResp.viewMaxFreqHz),
            {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Info}
        );
        if (s_sig.lastResp.selection.active) {
            ui.Text(
                ui.Format("Selection: Δt={:.3f}s | Peak={:.1f} dB | RMS={:.1f} dB | Fc={:.0f} Hz",
                          s_sig.lastResp.selection.DurationSec(),
                          s_sig.lastResp.selectionPeakDbFs,
                          s_sig.lastResp.selectionRmsDbFs,
                          s_sig.lastResp.selectionCentroidHz),
                {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Success}
            );
        }
        ui.Text(
            "Tip: Wheel=Zoom Time, Drag=2D Spectral Select, Alt+Drag=Pan, Double-Click Ruler=Add Cue",
            {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Muted}
        );
    }
}

void RenderSignalStage(guikit::Context& ui, DemoContext&) {
    EnsureSignalInitialized();
    StepSignalPlayback(s_sig);

    if (s_sig.livePlayhead) {
        ui.RequestRepaint(2);
    }

    if (auto col = ui.Column({
        .gap = 8.0f,
        .widthMode = guikit::SizeMode::Stretch,
        .heightMode = guikit::SizeMode::Stretch
    })) {
        if (auto tb = ui.Toolbar({.size = guikit::UiSize::Sm})) {
            if (tb.ToolButton({
                .label = s_sig.livePlayhead ? "LIVE PLAY" : "PAUSED",
                .icon = s_sig.livePlayhead ? guikit::Icon::Play() : guikit::Icon::Pause(),
                .variant = s_sig.livePlayhead ? guikit::UiVariant::Success : guikit::UiVariant::Warning,
                .active = s_sig.livePlayhead
            })) {
                s_sig.livePlayhead = !s_sig.livePlayhead;
            }

            tb.Separator();

            if (tb.ToolButton({
                .label = "Wave Top",
                .variant = (s_sig.panelOrder == guikit::SignalPanelOrder::WaveformTop) ? guikit::UiVariant::Primary : guikit::UiVariant::Ghost,
                .active = (s_sig.panelOrder == guikit::SignalPanelOrder::WaveformTop)
            })) {
                s_sig.panelOrder = guikit::SignalPanelOrder::WaveformTop;
            }
            if (tb.ToolButton({
                .label = "Spec Top",
                .variant = (s_sig.panelOrder == guikit::SignalPanelOrder::SpectrogramTop) ? guikit::UiVariant::Primary : guikit::UiVariant::Ghost,
                .active = (s_sig.panelOrder == guikit::SignalPanelOrder::SpectrogramTop)
            })) {
                s_sig.panelOrder = guikit::SignalPanelOrder::SpectrogramTop;
            }

            tb.Separator();

            if (tb.ToolButton({
                .label = "Linear Hz",
                .variant = (s_sig.frequencyScale == guikit::FrequencyScale::Linear) ? guikit::UiVariant::Info : guikit::UiVariant::Ghost,
                .active = (s_sig.frequencyScale == guikit::FrequencyScale::Linear)
            })) {
                s_sig.frequencyScale = guikit::FrequencyScale::Linear;
            }
            if (tb.ToolButton({
                .label = "Log Hz",
                .variant = (s_sig.frequencyScale == guikit::FrequencyScale::Logarithmic) ? guikit::UiVariant::Info : guikit::UiVariant::Ghost,
                .active = (s_sig.frequencyScale == guikit::FrequencyScale::Logarithmic)
            })) {
                s_sig.frequencyScale = guikit::FrequencyScale::Logarithmic;
            }

            tb.Spacer();

            if (tb.ToolButton({
                .label = "Zoom Selection",
                .icon = guikit::Icon::Search(),
                .disabled = !s_sig.selection.active || s_sig.selection.DurationSec() <= 1e-5
            })) {
                s_sig.requestViewStartSec = s_sig.selection.MinTimeSec();
                s_sig.requestViewEndSec = s_sig.selection.MaxTimeSec();
                if (s_sig.selection.hasSpectralBox && s_sig.selection.BandwidthHz() >= 50.0f) {
                    s_sig.requestFreqRangeHz = guikit::Vec2{s_sig.selection.MinFreqHz(), s_sig.selection.MaxFreqHz()};
                }
            }
            if (tb.ToolButton({
                .label = "Reset View",
                .icon = guikit::Icon::Refresh()
            })) {
                s_sig.requestResetView = true;
                s_sig.waveformGain = 1.0f;
            }
        }

        const uint32_t fftSize = SignalDemoState::fftSizeValues[s_sig.fftSizeIdx];
        s_sig.lastResp = ui.SignalView(s_sig.samples, {
            .sampleRate = s_sig.sampleRate,
            .panelOrder = s_sig.panelOrder,
            .splitRatio = s_sig.splitRatio,
            .amplitudeScale = s_sig.amplitudeScale,
            .frequencyScale = s_sig.frequencyScale,
            .colormap = s_sig.colormap,
            .fftWindow = s_sig.fftWindow,
            .fftSize = fftSize,
            .dbFloor = s_sig.dbFloor,
            .dbCeil = 0.0f,
            .waveformGain = s_sig.waveformGain,
            .showOverviewBar = s_sig.showOverviewBar,
            .showTimeRuler = s_sig.showTimeRuler,
            .showAmplitudeRuler = s_sig.showAmplitudeRuler,
            .showFrequencyRuler = s_sig.showFrequencyRuler,
            .showColorbar = s_sig.showColorbar,
            .showGrid = s_sig.showGrid,
            .showRms = s_sig.showRms,
            .showCrosshair = s_sig.showCrosshair,
            .splitRatioBinding = &s_sig.splitRatio,
            .waveformGainBinding = &s_sig.waveformGain,
            .playheadTimeSec = &s_sig.playheadSec,
            .selection = &s_sig.selection,
            .markers = &s_sig.markers,
            .requestViewStartSec = s_sig.requestViewStartSec,
            .requestViewEndSec = s_sig.requestViewEndSec,
            .requestFreqRangeHz = s_sig.requestFreqRangeHz,
            .requestResetView = s_sig.requestResetView,
            .heightMode = guikit::SizeMode::Stretch
        });

        s_sig.requestResetView = false;
        s_sig.requestViewStartSec.reset();
        s_sig.requestViewEndSec.reset();
        s_sig.requestFreqRangeHz.reset();
    }
}

std::string GenerateSignalCode(const DemoContext&) {
    static constexpr std::array<std::string_view, 2> orderEnums = {
        "WaveformTop", "SpectrogramTop"
    };
    static constexpr std::array<std::string_view, 2> ampEnums = {
        "dBFS", "Linear"
    };
    static constexpr std::array<std::string_view, 2> freqEnums = {
        "Linear", "Logarithmic"
    };
    static constexpr std::array<std::string_view, 5> cmapEnums = {
        "Inferno", "Magma", "Viridis", "CyanHot", "Grayscale"
    };
    static constexpr std::array<std::string_view, 4> winEnums = {
        "Hann", "Hamming", "BlackmanHarris", "Rectangular"
    };

    const uint32_t fftSize = SignalDemoState::fftSizeValues[s_sig.fftSizeIdx];
    return std::format(
        "// Synchronized DAW Waveform (Mipmap Pyramid) + STFT Spectrogram Analyzer\n"
        "auto resp = ui.SignalView(samples, {{\n"
        "    .sampleRate = {:.1f}f,\n"
        "    .panelOrder = guikit::SignalPanelOrder::{},\n"
        "    .splitRatio = {:.2f}f,\n"
        "    .amplitudeScale = guikit::AmplitudeScale::{},\n"
        "    .frequencyScale = guikit::FrequencyScale::{},\n"
        "    .colormap = guikit::SpectrogramColormap::{},\n"
        "    .fftWindow = guikit::FftWindowType::{},\n"
        "    .fftSize = {}u,\n"
        "    .dbFloor = {:.1f}f,\n"
        "    .waveformGain = {:.2f}f,\n"
        "    .showOverviewBar = {},\n"
        "    .showTimeRuler = {},\n"
        "    .showAmplitudeRuler = {},\n"
        "    .showFrequencyRuler = {},\n"
        "    .showColorbar = {},\n"
        "    .showRms = {},\n"
        "    .splitRatioBinding = &splitRatio,\n"
        "    .playheadTimeSec = &playheadSec,\n"
        "    .selection = &selection,\n"
        "    .markers = &cueMarkers,\n"
        "    .heightMode = guikit::SizeMode::Stretch\n"
        "}});\n",
        s_sig.sampleRate,
        orderEnums[static_cast<size_t>(s_sig.panelOrder)],
        s_sig.splitRatio,
        ampEnums[static_cast<size_t>(s_sig.amplitudeScale)],
        freqEnums[static_cast<size_t>(s_sig.frequencyScale)],
        cmapEnums[static_cast<size_t>(s_sig.colormap)],
        winEnums[static_cast<size_t>(s_sig.fftWindow)],
        fftSize,
        s_sig.dbFloor,
        s_sig.waveformGain,
        s_sig.showOverviewBar ? "true" : "false",
        s_sig.showTimeRuler ? "true" : "false",
        s_sig.showAmplitudeRuler ? "true" : "false",
        s_sig.showFrequencyRuler ? "true" : "false",
        s_sig.showColorbar ? "true" : "false",
        s_sig.showRms ? "true" : "false"
    );
}

} // namespace guikit::demo
