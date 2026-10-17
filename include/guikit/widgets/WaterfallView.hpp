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

/// Domain operating mode for the WaterfallView analyzer & tracker.
enum class WaterfallMode : uint8_t {
    SpectrumRtsa = 0,   ///< Real-Time Spectrum Analyzer (RTSA): Frequency (Hz/kHz/MHz) vs Scrolling Time (s)
    DoaBtr              ///< Bearing-Time Record (BTR) / Direction-Of-Arrival (DOA) Tracker: Azimuth (deg) vs Time (s)
};

/// Panel arrangement within the WaterfallView viewport.
enum class WaterfallLayout : uint8_t {
    SplitTraceAndWaterfall = 0, ///< Upper instantaneous trace (Live + Peak Hold + Avg + Gate) + Lower scrolling Waterfall
    WaterfallOnly,              ///< Full-height scrolling Waterfall heatmap
    TraceOnly                   ///< Full-height instantaneous Spectrum / DOA Spatial Response trace plot
};

/// Tracking state of an automated DOA contact or frequency-agile emitter track.
enum class WaterfallTrackStatus : uint8_t {
    Tracking = 0,       ///< Locked active target track with high SNR
    Coasting,           ///< Memory-predicted track during temporary signal fade
    Alarm               ///< Priority alert / sector intrusion / high-SNR emitter threat
};

/// Historical trajectory vertex for a tracked DOA target or drifting spectral emitter.
struct WaterfallTrackPoint {
    float timeAgoSec{0.0f}; ///< Relative time in the past [0 .. historyDurationSec]
    float xValue{0.0f};     ///< Bearing in degrees (DOA mode) or Frequency in Hz (Spectrum mode)
    float snrDb{0.0f};      ///< Instantaneous SNR in dB at that historical timestamp
};

/// Tracked DOA target (BTR contact) or spectral emitter trajectory overlaid on the waterfall & top trace.
struct WaterfallTrack {
    int id{0};
    std::string callsign{};
    WaterfallTrackStatus status{WaterfallTrackStatus::Tracking};
    float currentX{0.0f};               ///< Current Bearing (deg) or Frequency (Hz) at t = 0s
    float driftRatePerSec{0.0f};        ///< Bearing drift rate d(theta)/dt (deg/s) or Doppler rate df/dt (Hz/s)
    float levelDb{-24.0f};              ///< Peak signal level in dB
    float snrDb{18.0f};                 ///< Signal-to-Noise Ratio above local noise floor in dB
    float gateWidth{8.0f};              ///< Tracking gate / association corridor width in X units (deg or Hz)
    std::vector<WaterfallTrackPoint> history{}; ///< Trajectory points ordered from newest (0s) to oldest
};

/// Configuration options for the autonomous WaterfallView (RTSA Spectrum & DOA/BTR Tracker) widget.
struct WaterfallViewOptions {
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

    // --- Domain Mode, Layout & Ring-Buffer Geometry ---
    WaterfallMode mode{WaterfallMode::SpectrumRtsa};
    WaterfallLayout viewLayout{WaterfallLayout::SplitTraceAndWaterfall};
    Colormap colormap{Colormap::Turbo};

    uint32_t numBins{512};                          ///< Number of X bins per slice (frequency bins or azimuth beams)
    uint32_t historyRows{300};                      ///< Circular ring-buffer capacity in rows (e.g. 180..1024)
    float historyDurationSec{30.0f};                ///< Total historical time span in seconds corresponding to `historyRows`
    float minX{0.0f};                               ///< Full-scale minimum X (e.g. 0 Hz or -180 deg)
    float maxX{24000.0f};                           ///< Full-scale maximum X (e.g. 24000 Hz or +180 deg)
    float topTraceHeightRatio{0.30f};               ///< Height ratio [0.18 .. 0.55] of upper trace panel in Split mode
    bool invertTimeAxis{false};                     ///< false = 0s at top flowing down; true = 0s at bottom flowing up

    // --- Streaming & Snapshot Data Ingestion ---
    uint64_t sliceSequence{0};                      ///< Monotonic counter; when incremented, `latestSliceDb` is pushed as a new row
    std::span<const float> initialHistoryMatrix{};  ///< Optional [historyRows * numBins] dB matrix (row 0 = newest) for bulk preload

    // --- DSP & Display Conditioning Parameters ---
    float dbFloor{-110.0f};                         ///< Display dynamic range floor in dB (-140 .. -20 dB)
    float dbCeil{0.0f};                             ///< Display dynamic range ceiling in dB (-60 .. +20 dB)
    float gainDb{0.0f};                             ///< Digital display gain offset in dB (-30 .. +30 dB)
    float thresholdGateDb{-62.0f};                  ///< Detection / squelch threshold level in dB
    float gateSuppression{0.35f};                   ///< GPU noise-floor suppression below `thresholdGateDb` [0.0 .. 1.0]
    float gamma{1.0f};                              ///< Colormap contrast gamma exponent [0.4 .. 2.2]
    float averageAlpha{0.12f};                      ///< Exponential smoothing factor for the RMS/Average trace [0.01 .. 0.5]
    float peakDecayDbPerSec{1.2f};                  ///< Peak-hold decay rate in dB/sec (0.0 = infinite max hold)

    // --- Traces, Target Tracks & Operator Instruments ---
    std::span<const WaterfallTrack> tracks{};       ///< Active DOA contacts (BTR) or tracked spectral emitters
    bool showLiveTrace{true};                       ///< Render instantaneous live slice in upper trace panel
    bool showPeakHold{true};                        ///< Render peak-hold envelope trace in upper trace panel
    bool showAverageTrace{true};                    ///< Render exponentially averaged trace in upper trace panel
    bool showThresholdGate{true};                   ///< Render interactive detection threshold line in upper trace panel
    bool showPeakMarkers{true};                     ///< Automatically detect and label dominant peaks / DOA lobes
    bool showTracks{true};                          ///< Render DOA / emitter trajectory curves and vectors over waterfall
    bool showTrackGates{true};                      ///< Render tracking gate corridors and callsign tags
    bool showHarmonicComb{false};                   ///< Render fundamental f0 and harmonic comb markers (2f0..6f0)
    bool showColorbar{true};                        ///< Render right-hand dB intensity colorbar with threshold marker
    bool showHudOverlay{true};                      ///< Render corner operator telemetry HUD cards
    bool showGrid{true};                            ///< Render frequency/bearing and time graticule lines

    // --- Two-Way Interactive State Bindings ---
    int* selectedTrackId{nullptr};                  ///< Optional external ID of selected track (-1 = none)
    float* viewMinX{nullptr};                       ///< Optional external visible X start (wheel zoom / drag pan)
    float* viewMaxX{nullptr};                       ///< Optional external visible X end (wheel zoom / drag pan)
    float* selectionStartX{nullptr};                ///< Optional external band/sector selection start X
    float* selectionEndX{nullptr};                  ///< Optional external band/sector selection end X
    float* harmonicFundamentalX{nullptr};           ///< Optional external fundamental frequency f0 for harmonic comb
    float* thresholdGateDbPtr{nullptr};             ///< Optional external two-way binding for interactive threshold gate

    // --- Programmatic Actions ---
    bool requestResetZoom{false};
    bool requestResetPeakHold{false};
    bool requestClearHistory{false};

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

/// Rich telemetry & operator interaction state returned by `ui.WaterfallView(...)`.
struct WaterfallViewResponse : public Response {
    WaterfallMode mode{WaterfallMode::SpectrumRtsa};
    float visibleMinX{0.0f};
    float visibleMaxX{24000.0f};
    float zoomFactor{1.0f};

    bool cursorInside{false};
    bool cursorInWaterfall{false};
    bool cursorInTrace{false};
    float cursorX{0.0f};                ///< Hovered Frequency (Hz) or Bearing (deg)
    float cursorTimeAgoSec{0.0f};       ///< Hovered historical timestamp (-dt in seconds)
    float cursorWaterfallDb{-110.0f};   ///< Historical dB value at (cursorX, cursorTimeAgoSec)
    float cursorLiveDb{-110.0f};        ///< Current live slice dB at cursorX
    float cursorPeakDb{-110.0f};        ///< Peak-hold envelope dB at cursorX
    float cursorAvgDb{-110.0f};         ///< Exponentially averaged dB at cursorX

    bool hasSelectionBand{false};
    float selectionStartX{0.0f};
    float selectionEndX{0.0f};
    float selectionCenterX{0.0f};
    float selectionSpanX{0.0f};
    float selectionBandPowerDb{-110.0f}; ///< Integrated power / peak dB inside selected band or sector

    int hoveredTrackId{-1};
    int selectedTrackId{-1};
    bool trackSelectionChanged{false};

    float dominantPeakX{0.0f};
    float dominantPeakDb{-110.0f};
    uint32_t populatedRows{0};
};

/// Autonomous Hardware-Accelerated Waterfall (RTSA Spectrum Analyzer & DOA / BTR Target Tracker) widget.
class WaterfallView {
public:
    static WaterfallViewResponse Run(
        Context& ctx,
        std::span<const float> latestSliceDb,
        const WaterfallViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] static Vec2 Measure(
        std::span<const float> latestSliceDb,
        const WaterfallViewOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        const UiTheme& theme = {},
        float availableWidth = 0.0f
    );
};

} // namespace guikit
