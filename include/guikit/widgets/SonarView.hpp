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

/// Display geometry mode for the SonarView indicator.
enum class SonarDisplayMode : uint8_t {
    PPI_360 = 0,        ///< Full 360-degree Plan Position Indicator (circular harbor/vessel defense radar & sonar)
    SectorWedge,        ///< Forward-looking multibeam fan / wedge (e.g. 60..180 deg Diver Detection Sonar)
    BScan               ///< Rectilinear Azimuth (X) vs Range (Y) unwrapped polar matrix inspector
};

/// Scan update & persistence mode for the polar acoustic/radar display.
enum class SonarScanMode : uint8_t {
    MultibeamContinuous = 0, ///< Full-sector electronic multibeam ping update with smooth persistence
    RotatingSweep            ///< Rotating or sector-sweeping beam with exponential phosphor afterglow
};

/// Classification of a tracked underwater or surface contact (ARPA / Diver Detection tracker).
enum class SonarTargetClass : uint8_t {
    Diver = 0,          ///< Submerged swimmer / combat diver threat (high-priority diamond reticle)
    UUV,                ///< Unmanned Underwater Vehicle / ROV / mini-sub (hexagonal/directional reticle)
    SurfaceCraft,       ///< Surface vessel / tender boat (circular ARPA symbol)
    Biologic,           ///< Fish school / marine mammal / non-threat biologic clutter
    StaticAnomaly       ///< Mooring, seabed debris, or mine-like bottom contact
};

/// Polar coordinate pair (Range in meters, Azimuth Bearing in degrees [0..360)).
struct PolarPoint {
    float rangeMeters{0.0f};
    float bearingDeg{0.0f};
};

/// Tracked contact / target rendered on the tactical overlay with velocity vector and snail-trail history.
struct SonarTarget {
    int id{0};
    std::string callsign{};
    SonarTargetClass classification{SonarTargetClass::Diver};
    float rangeMeters{100.0f};          ///< Distance from transducer / radar origin in meters
    float bearingDeg{0.0f};             ///< Azimuth angle in degrees (0 = North / Dead Ahead, clockwise)
    float speedMetersPerSec{0.8f};      ///< Target speed over ground in m/s
    float courseDeg{180.0f};            ///< Target heading / course over ground in degrees
    float depthMeters{6.0f};            ///< Estimated target depth in meters (for 3D/DDS sonar)
    float snrDb{18.0f};                 ///< Target signal-to-noise ratio in dB
    std::vector<PolarPoint> trail{};    ///< Historical polar positions (snail trail)
};

/// Tactical perimeter / exclusion sector that triggers an intrusion alarm when entered by a threat target.
struct SonarGuardZone {
    bool enabled{true};
    float minRangeMeters{80.0f};
    float maxRangeMeters{320.0f};
    float startBearingDeg{310.0f};      ///< Sector start angle in degrees [0..360)
    float endBearingDeg{50.0f};         ///< Sector end angle in degrees [0..360) (supports wrap-around across 0 deg)
};

/// Configuration options for the autonomous SonarView (Polar Heatmap / Diver Detection Sonar / PPI Radar) widget.
struct SonarViewOptions {
    // --- Element Companion Header / Footer ---
    UiText label{};
    UiText description{};
    Icon icon{};
    UiText badge{};
    UiVariant variant{UiVariant::Default};
    ElementLayout layout{ElementLayout::Stacked};
    InlineOrder inlineOrder{InlineOrder::LabelFirst};
    Align align{Align::Start};

    // --- Polar Matrix Dimensions & Geometry ---
    uint32_t numBeams{180};                         ///< Number of angular beams (rows) in `polarMatrix`
    uint32_t numRangeBins{256};                     ///< Number of range bins (columns) per beam in `polarMatrix`
    SonarDisplayMode displayMode{SonarDisplayMode::PPI_360};
    SonarScanMode scanMode{SonarScanMode::MultibeamContinuous};
    Colormap colormap{Colormap::Gold};

    float maxRangeMeters{500.0f};                   ///< Full-scale acoustic/radar range corresponding to bin `numRangeBins - 1`
    float minRangeMeters{0.0f};                     ///< Minimum blind-zone range in meters
    float headingDeg{0.0f};                         ///< Sensor center boresight / vessel heading in degrees
    float sectorApertureDeg{120.0f};                ///< Angular fan width in degrees when in `SectorWedge` mode (e.g. 60..180)
    float matrixStartBearingDeg{0.0f};              ///< Azimuth of beam 0 in `polarMatrix`
    float matrixEndBearingDeg{360.0f};              ///< Azimuth of last beam in `polarMatrix`

    // --- Hydroacoustic / Radar DSP Parameters ---
    float gainDb{0.0f};                             ///< Receiver gain offset in dB (-20 .. +40 dB)
    float tvgAmount{0.55f};                         ///< Time-Varied Gain (0.0 .. 1.0) compensating range attenuation
    float clutterThreshold{0.12f};                  ///< Noise/reverberation floor cutoff [0.0 .. 0.85]
    float persistence{0.82f};                       ///< Temporal frame-to-frame phosphor/acoustic persistence [0.0 .. 0.97]
    float sweepAngleDeg{0.0f};                      ///< Current active sweep beam azimuth (for `RotatingSweep` mode)
    float sweepBeamWidthDeg{18.0f};                 ///< Angular width of the active sweep update wedge

    // --- Tactical Overlay & Instruments ---
    std::span<const SonarTarget> targets{};         ///< Active ARPA / Diver Detection tracked contacts
    SonarGuardZone guardZone{};                     ///< Optional perimeter alarm sector
    bool showRangeRings{true};                      ///< Concentric range rings/arcs with distance readouts
    bool showBearingDial{true};                     ///< Outer compass rose / azimuth degree ticks and spokes
    bool showTargets{true};                         ///< Render ARPA / Diver target symbols, trails, and velocity vectors
    bool showTargetLabels{true};                    ///< Render callsign, range/bearing, and depth tags next to targets
    bool showColorbar{true};                        ///< Right-side acoustic intensity (dB) colorbar & threshold gauge
    bool showEblVrm{true};                          ///< Interactive Electronic Bearing Line (EBL) & Variable Range Marker (VRM)
    bool showHudOverlay{true};                      ///< Tactical corner HUD telemetry cards (sensor status, selected target, alarm)
    uint32_t rangeRingCount{5};                     ///< Number of concentric range divisions

    // --- Two-Way Interactive State Bindings ---
    int* selectedTargetId{nullptr};                 ///< Optional external ID of currently selected/locked target (-1 = none)
    float* eblBearingDeg{nullptr};                  ///< Optional external Electronic Bearing Line angle in degrees
    float* vrmRangeMeters{nullptr};                 ///< Optional external Variable Range Marker distance in meters
    float* viewRangeZoom{nullptr};                  ///< Optional external range zoom multiplier (0.25 .. 1.0, wheel-zoomable)

    // --- Programmatic View Requests ---
    bool requestResetView{false};
    bool requestClearPersistence{false};

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

/// Rich tactical & hydroacoustic telemetry returned by `ui.SonarView(...)`.
struct SonarViewResponse : public Response {
    SonarDisplayMode displayMode{SonarDisplayMode::PPI_360};
    float visibleMaxRangeMeters{500.0f};
    float rangeZoom{1.0f};

    bool cursorInsidePlot{false};
    float cursorRangeMeters{0.0f};
    float cursorBearingDeg{0.0f};
    float cursorEchoDb{-60.0f};

    bool eblVrmActive{false};
    float eblBearingDeg{45.0f};
    float vrmRangeMeters{250.0f};

    int hoveredTargetId{-1};
    int selectedTargetId{-1};
    bool targetSelectionChanged{false};

    bool guardZoneAlarm{false};
    uint32_t guardZoneIntruderCount{0};
};

/// Autonomous Polar Heatmap / Diver Detection Sonar (DDS) / Marine PPI Radar widget.
class SonarView {
public:
    static SonarViewResponse Run(
        Context& ctx,
        std::span<const float> polarMatrix,
        const SonarViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] static Vec2 Measure(
        std::span<const float> polarMatrix,
        const SonarViewOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        const UiTheme& theme = {},
        float availableWidth = 0.0f
    );
};

} // namespace guikit
