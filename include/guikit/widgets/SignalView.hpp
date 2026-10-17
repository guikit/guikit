#pragma once

#include <cstdint>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Colormap.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Vertical arrangement order of the synchronized Waveform and Spectrogram panels.
enum class SignalPanelOrder : uint8_t {
    WaveformTop = 0,    ///< Waveform on top, Spectrogram on bottom
    SpectrogramTop      ///< Spectrogram on top, Waveform on bottom
};

/// Vertical scale mode for the Waveform amplitude ruler and plot.
enum class AmplitudeScale : uint8_t {
    dBFS = 0,           ///< Logarithmic decibels full scale (0 dB, -6 dB, -12 dB, -24 dB, -48 dB, -inf)
    Linear              ///< Linear normalized sample amplitude (-1.0 .. 0.0 .. +1.0)
};

/// Vertical frequency scale mode for the Spectrogram frequency ruler and heatmap.
enum class FrequencyScale : uint8_t {
    Linear = 0,         ///< Linear frequency distribution (0 Hz .. Fs/2)
    Logarithmic         ///< Logarithmic / psychoacoustic frequency distribution (minFreq .. Fs/2)
};

/// Windowing function applied to each Short-Time Fourier Transform (STFT) frame.
enum class FftWindowType : uint8_t {
    Hann = 0,           ///< Raised-cosine Hann window (balanced main-lobe width & sidelobe roll-off)
    Hamming,            ///< Hamming window (suppressed nearest sidelobe)
    BlackmanHarris,     ///< 4-term Blackman-Harris window (>92 dB sidelobe suppression for high dynamic range)
    Rectangular         ///< Unwindowed rectangular frame (maximum narrow main-lobe, highest spectral leakage)
};

/// Interactive timeline cue marker displayed on the time ruler and across signal panels.
struct SignalMarker {
    double timeSec{0.0};
    std::string label{};
    Color color{245, 158, 11, 255};
};

/// Time and optional 2D spectral selection region (iZotope RX / Adobe Audition style).
struct SignalSelection {
    bool active{false};
    double startTimeSec{0.0};
    double endTimeSec{0.0};
    bool hasSpectralBox{false};     ///< True when a 2D [t0..t1] x [f0..f1] frequency band is selected
    float lowFreqHz{0.0f};
    float highFreqHz{0.0f};

    [[nodiscard]] constexpr double MinTimeSec() const noexcept {
        return startTimeSec < endTimeSec ? startTimeSec : endTimeSec;
    }
    [[nodiscard]] constexpr double MaxTimeSec() const noexcept {
        return startTimeSec > endTimeSec ? startTimeSec : endTimeSec;
    }
    [[nodiscard]] constexpr double DurationSec() const noexcept {
        return active ? (MaxTimeSec() - MinTimeSec()) : 0.0;
    }
    [[nodiscard]] constexpr float MinFreqHz() const noexcept {
        return lowFreqHz < highFreqHz ? lowFreqHz : highFreqHz;
    }
    [[nodiscard]] constexpr float MaxFreqHz() const noexcept {
        return lowFreqHz > highFreqHz ? lowFreqHz : highFreqHz;
    }
    [[nodiscard]] constexpr float BandwidthHz() const noexcept {
        return (active && hasSpectralBox) ? (MaxFreqHz() - MinFreqHz()) : 0.0f;
    }
};

/// Configuration options for the autonomous SignalView (Waveform + Spectrogram DAW analyzer) widget.
struct SignalViewOptions {
    // --- Element Companion Header / Footer ---
    UiText label{};
    UiText description{};
    Icon icon{};
    UiText badge{};
    UiVariant variant{UiVariant::Default};
    ElementLayout layout{ElementLayout::Stacked};
    InlineOrder inlineOrder{InlineOrder::LabelFirst};
    Align elementAlign{Align::Start};
    bool divider{false};
    ElementDivider dividerStyle{ElementDivider::Bottom};

    // --- Signal & DSP Parameters ---
    float sampleRate{48000.0f};
    SignalPanelOrder panelOrder{SignalPanelOrder::WaveformTop};
    float splitRatio{0.46f};                        ///< Fraction of plot height assigned to top panel [0.0 .. 1.0]
    AmplitudeScale amplitudeScale{AmplitudeScale::dBFS};
    FrequencyScale frequencyScale{FrequencyScale::Linear};
    Colormap colormap{Colormap::Inferno};
    FftWindowType fftWindow{FftWindowType::Hann};
    uint32_t fftSize{512};                          ///< Radix-2 FFT size (256, 512, 1024, 2048)
    float dbFloor{-90.0f};                          ///< Spectrogram minimum power floor in dBFS
    float dbCeil{0.0f};                             ///< Spectrogram ceiling in dBFS
    float waveformGain{1.0f};                       ///< Default vertical amplitude zoom factor (1.0 = 0 dB)

    // --- Visual & Interactive Features ---
    bool showOverviewBar{true};                     ///< Top full-signal minimap navigator with draggable zoom lens
    bool showTimeRuler{true};                       ///< Adaptive time / sample ruler with cue markers
    bool showAmplitudeRuler{true};                  ///< Interactive left amplitude ruler for Waveform (wheel/drag zoom)
    bool showFrequencyRuler{true};                  ///< Interactive left frequency ruler for Spectrogram (wheel/drag zoom)
    bool showColorbar{true};                        ///< Right-hand dBFS heatmap legend & dynamic range adjuster
    bool showGrid{true};                            ///< Time, amplitude, and frequency grid lines
    bool showRms{true};                             ///< Render inner RMS energy core inside peak waveform envelope
    bool showCrosshair{true};                       ///< Synchronized crosshair & live cursor readout HUD
    bool allowSpectralSelection{true};              ///< Enable 2D [time x frequency] marquee box selection on spectrogram
    bool allowMarkerEditing{true};                  ///< Double-click time ruler to add/delete cue markers, drag to move

    // --- Optional Two-Way External Bindings ---
    float* splitRatioBinding{nullptr};              ///< Optional external float updated when user drags the splitter
    float* waveformGainBinding{nullptr};            ///< Optional external float updated when user zooms amplitude ruler
    double* playheadTimeSec{nullptr};               ///< Optional external playhead timestamp in seconds
    SignalSelection* selection{nullptr};            ///< Optional external time/spectral selection state
    std::vector<SignalMarker>* markers{nullptr};    ///< Optional external cue markers vector

    // --- Programmatic Camera / View Requests ---
    std::optional<double> requestViewStartSec{std::nullopt};
    std::optional<double> requestViewEndSec{std::nullopt};
    std::optional<Vec2> requestFreqRangeHz{std::nullopt};
    bool requestResetView{false};

    // --- Sizing & Styling ---
    StretchMode stretch{StretchMode::Fill};
    std::optional<float> width{std::nullopt};
    std::optional<float> height{std::nullopt};
    SizeMode heightMode{SizeMode::Stretch};
    std::optional<float> borderRadius{std::nullopt};
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
    std::string_view tooltip{};
};

/// Rich telemetry & interaction response returned by `ui.SignalView(...)`.
struct SignalViewResponse : public Response {
    double viewStartSec{0.0};
    double viewEndSec{0.0};
    double totalDurationSec{0.0};
    size_t totalSamples{0};
    float splitRatio{0.46f};
    float waveformGain{1.0f};
    float viewMinFreqHz{0.0f};
    float viewMaxFreqHz{24000.0f};
    double playheadSec{0.0};
    SignalSelection selection{};

    bool cursorInsidePlot{false};
    double cursorTimeSec{0.0};
    size_t cursorSampleIndex{0};
    float cursorAmplitude{0.0f};
    float cursorDbFs{-120.0f};
    float cursorFreqHz{0.0f};
    float cursorSpectralDb{-120.0f};

    float selectionPeakDbFs{-120.0f};
    float selectionRmsDbFs{-120.0f};
    float selectionCentroidHz{0.0f};

    int hoveredMarkerIndex{-1};
    int activeMarkerIndex{-1};
};

/// Autonomous DAW-grade Waveform + Spectrogram signal analyzer widget.
class SignalView {
public:
    static SignalViewResponse Run(
        Context& ctx,
        std::span<const float> samples,
        const SignalViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] static Vec2 Measure(
        std::span<const float> samples,
        const SignalViewOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        const UiTheme& theme = {},
        float availableWidth = 0.0f
    );
};

} // namespace guikit
