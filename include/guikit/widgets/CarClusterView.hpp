#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <source_location>
#include <string_view>
#include "guikit/core/Control.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Instrument cluster architectural layout mode for `CarClusterView`.
enum class ClusterLayoutMode : uint8_t {
    DualDial = 0,       ///< Classic Digital Cockpit: Left Speedometer dial + Center MFD + Right Tachometer / Power-Regen dial
    TrackCentral,       ///< Motorsport / GT Mode: Dominant central Tachometer + digital speed/gear + Left & Right telemetry wings
    ModernWide          ///< Panoramic EV / ADAS Cockpit: Wide central 3D lane & radar stage + sweeping side arc gauges
};

/// Powertrain architecture mode controlling tachometer vs power/regen metering and thermal/energy readouts.
enum class PowertrainMode : uint8_t {
    IceTurbo = 0,       ///< Internal Combustion Turbo + 8-speed DCT/PDK (RPM 0..8500, Redline, Gear 1..8, Boost bar, Oil Temp, Fuel)
    ElectricDualMotor   ///< Performance Dual-Motor EV (-150 kW Regen .. 0 .. +450 kW Power, SoC %, Battery Temp, kWh/100km)
};

/// Visual color theme and drive mode styling for the digital instrument cluster.
enum class ClusterDriveTheme : uint8_t {
    ComfortCyan = 0,    ///< Deep slate glass cockpit with crisp ice-cyan and cool white accents
    SportCrimson,       ///< Carbon-black performance theme with aggressive crimson red and amber highlights
    EcoEmerald,         ///< Efficiency-focused emerald green and aqua-teal palette
    TrackGold           ///< High-contrast motorsport gold and titanium telemetry palette
};

/// Active page displayed inside the central Multi-Function Display (MFD) viewport.
enum class ClusterCenterMfd : uint8_t {
    AdasLaneRadar = 0,  ///< 3D ADAS highway lane perspective, dynamic steering path, radar lead car & ACC distance
    TpmsChassis,        ///< 4-wheel chassis vector diagram with per-wheel tire pressure (bar/psi) and temperature
    GForceDynamics      ///< 2D Lateral/Longitudinal G-Meter accelerometer reticle, steering angle, AWD split & boost/power
};

/// Measurement unit system for speed, distance, temperature, and pressure readouts.
enum class SpeedUnit : uint8_t {
    Kmh = 0,            ///< Metric: km/h, km, °C, bar, L/100km (or kWh/100km)
    Mph                 ///< Imperial: mph, mi, °F, psi, mpg (or mi/kWh)
};

/// Transmission selector state (`P R N D S M`).
enum class GearSelector : uint8_t {
    Park = 0,
    Reverse,
    Neutral,
    Drive,
    Sport,
    Manual
};

/// ISO automotive warning and status telltale indicator flags.
struct CarTelltaleFlags {
    bool leftTurn{false};
    bool rightTurn{false};
    bool hazard{false};
    bool lowBeam{true};
    bool highBeam{false};
    bool fogLight{false};
    bool seatbeltWarning{false};
    bool parkBrake{false};
    bool absWarning{false};
    bool escActive{false};
    bool tpmsWarning{false};
    bool batteryOrEngineAlert{false};
    bool adasCruiseActive{true};
    bool laneKeepActive{true};
    bool blinkPhaseOn{true};        ///< Periodic blink state for turn signals and shift-light flash
};

/// Declarative configuration options for the autonomous `CarClusterView` widget.
struct CarClusterViewOptions {
    Align align{Align::Start};

    // --- Cluster Architecture, Powertrain & Visual Theme ---
    ClusterLayoutMode layoutMode{ClusterLayoutMode::DualDial};
    PowertrainMode powertrain{PowertrainMode::IceTurbo};
    ClusterDriveTheme driveTheme{ClusterDriveTheme::ComfortCyan};
    ClusterCenterMfd centerMfd{ClusterCenterMfd::AdasLaneRadar};
    SpeedUnit speedUnit{SpeedUnit::Kmh};
    GearSelector gearSelector{GearSelector::Drive};

    // --- Primary Speed, RPM / Power & Drivetrain Telemetry ---
    float speedKmh{96.0f};              ///< Vehicle ground speed in km/h (automatically converted when `speedUnit == Mph`)
    float maxSpeedKmh{300.0f};          ///< Full-scale speedometer dial maximum in km/h (200 .. 360)
    float cruiseTargetKmh{110.0f};      ///< Adaptive Cruise Control (ACC) target speed bug in km/h
    bool cruiseSet{true};               ///< Render ACC speed bug on speedometer arc
    float rpm{2850.0f};                 ///< Engine speed in RPM (0 .. maxRpm) for `PowertrainMode::IceTurbo`
    float redlineRpm{7000.0f};          ///< Redline threshold in RPM where warning ticks & shift lights engage
    float maxRpm{8500.0f};              ///< Full-scale tachometer maximum in RPM (6000 .. 10000)
    int currentGear{5};                 ///< Active forward gear ratio (1 .. 8) when in D / S / M
    float evPowerKw{48.0f};             ///< Electric motor net power in kW (-150 kW Regen .. +450 kW Power)
    float throttlePct{22.0f};           ///< Accelerator pedal position in % (0 .. 100)
    float brakePct{0.0f};               ///< Brake pedal pressure in % (0 .. 100)
    float steeringAngleDeg{0.0f};       ///< Steering wheel angle in degrees (-45 .. +45, negative = left, positive = right)
    float boostBar{0.35f};              ///< Turbocharger relative boost pressure in bar (-0.6 .. +2.0 bar)

    // --- Energy, Thermal & Trip Computer Telemetry ---
    float fuelOrSocPct{76.0f};          ///< Fuel tank level or HV battery State of Charge (SoC) in % (0 .. 100)
    float estimatedRangeKm{445.0f};     ///< Remaining driving range in km
    float coolantOrBattTempC{90.0f};    ///< Engine coolant or HV battery pack temperature in °C
    float oilTempC{98.0f};              ///< Engine / transmission oil temperature in °C
    float outsideTempC{18.5f};          ///< Ambient exterior temperature in °C
    double odometerKm{18420.6};         ///< Total vehicle odometer in km
    float tripDistanceKm{74.8f};        ///< Current trip distance in km
    float avgConsumption{9.4f};         ///< Average consumption (L/100km for ICE, kWh/100km for EV)

    // --- Chassis Dynamics, ADAS Radar & TPMS Telemetry ---
    float latG{0.0f};                   ///< Lateral acceleration in G (-1.8 .. +1.8 G)
    float lonG{0.0f};                   ///< Longitudinal acceleration in G (-1.8 .. +1.8 G, positive = accel, negative = brake)
    float peakG{0.45f};                 ///< Peak combined G-load recorded during session
    float frontTorquePct{32.0f};        ///< AWD front axle torque distribution in % (0 .. 100)
    float adasLeadDistanceM{44.0f};     ///< Radar distance to lead vehicle in meters (5 .. 120 m)
    float adasLeadSpeedKmh{96.0f};      ///< Lead vehicle speed in km/h
    bool adasLeadDetected{true};        ///< Whether a lead vehicle is locked by front radar
    std::array<float, 4> tirePressureBar{2.4f, 2.4f, 2.6f, 2.6f}; ///< Tire pressures [FL, FR, RL, RR] in bar
    std::array<float, 4> tireTempC{44.0f, 45.0f, 48.0f, 48.0f};   ///< Tire temperatures [FL, FR, RL, RR] in °C

    // --- Sub-Instrument & Overlay Visibility Toggles ---
    bool showShiftLights{true};         ///< Render top RPM / Power sequential LED shift-light strip
    bool showTelltaleBar{true};         ///< Render ISO automotive warning/status telltale icons header
    bool showBottomTripBar{true};       ///< Render bottom PRNDSM gear ribbon, odometer, trip & ambient status bar
    bool showMouseDriveOverlay{false};  ///< Render interactive mouse pedal/steering HUD reticle
    CarTelltaleFlags telltales{};

    // --- Optional Two-Way Bindings ---
    int* manualGearPtr{nullptr};        ///< Optional binding: mouse wheel steps gear 1..8 when hovered
    ClusterCenterMfd* centerMfdPtr{nullptr}; ///< Optional binding: clicking MFD page dots cycles MFD screen

    // --- Sizing & Styling ---
    StretchMode stretch{StretchMode::Fill};
    std::optional<float> width{std::nullopt};
    std::optional<float> height{std::nullopt};
    SizeMode heightMode{SizeMode::Stretch};
    StateColor background{};
    Border border{};
    bool disabled{false};
    std::string_view tooltip{};
};

/// Rich telemetry geometry and interactive mouse-drive state returned by `ui.CarClusterView(...)`.
struct CarClusterViewResponse : public Response {
    ClusterLayoutMode layoutMode{ClusterLayoutMode::DualDial};
    ClusterCenterMfd activeCenterMfd{ClusterCenterMfd::AdasLaneRadar};
    Rect clusterRect{};                 ///< Resolved inner binnacle screen rectangle
    Rect leftGaugeRect{};               ///< Left dial / wing bounding rectangle
    Rect rightGaugeRect{};              ///< Right dial / wing bounding rectangle
    Rect centerMfdRect{};               ///< Central MFD viewport bounding rectangle

    bool cursorInside{false};           ///< True when mouse cursor is inside the cluster viewport
    Vec2 cursorNorm{0.0f, 0.0f};        ///< Normalized cursor offset from center in [-1.0, +1.0] (X = steer, Y = pedal)
    float cursorSteerDeg{0.0f};         ///< Steering angle mapped from cursor X (-45 .. +45 deg)
    float cursorThrottlePct{0.0f};      ///< Throttle pedal % mapped from cursor Y above center (0 .. 100 %)
    float cursorBrakePct{0.0f};         ///< Brake pedal % mapped from cursor Y below center (0 .. 100 %)
};

/// Autonomous Digital Automotive Instrument Cluster & ADAS/Telemetry Cockpit widget.
class CarClusterView {
public:
    static CarClusterViewResponse Run(
        Context& ctx,
        const CarClusterViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] static Vec2 Measure(
        const CarClusterViewOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        const UiTheme& theme = {},
        float availableWidth = 0.0f
    );
};

} // namespace guikit
