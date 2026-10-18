#pragma once

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

/// Display layout mode for the AttitudeView (Artificial Horizon / Primary Flight Display) widget.
enum class AttitudeDisplayMode : uint8_t {
    FullPFD = 0,        ///< Complete Primary Flight Display: ADI horizon + IAS speed tape + Altitude & VSI tapes + Heading tape + FMA bar + ILS diamonds
    CompactADI,         ///< Standalone circular Attitude Director Indicator (artificial horizon dial with digital corner readouts)
    HudOverlay          ///< Transparent Head-Up Display (HUD) collimator vector symbology without opaque sky/ground fill
};

/// Coordinate reference frame for the artificial horizon and aircraft symbol.
enum class HorizonReferenceMode : uint8_t {
    WesternMovingHorizon = 0, ///< Western ("Inside-Out" / View from cockpit): Fixed aircraft symbol, horizon rolls and pitches
    EasternMovingAircraft     ///< Russian KPPI / AGD ("Outside-In" / View from ground): Horizon pitches vertically while aircraft silhouette rolls
};

/// Color palette and avionics visual styling for the AttitudeView instrument.
enum class AttitudeColorScheme : uint8_t {
    GlassCockpit = 0,   ///< Classic avionics sky blue & earth brown (Garmin G1000 / Airbus / Boeing PFD style)
    HudGreen,           ///< High-contrast monochrome phosphor green HUD symbology
    TacticalDark        ///< Modern dark synth-vision telemetry theme matching the GuiKit dark palette
};

/// Flight Director (FD) command cue symbology mode.
enum class FlightDirectorStyle : uint8_t {
    Off = 0,            ///< Flight Director hidden
    SingleCueVBar,      ///< Single-cue magenta V-Bar wedge command indicator
    DualCueCrossbars    ///< Dual-cue magenta pitch & roll crossbar needles
};

/// Central fixed/rolling aircraft reference symbol style.
enum class AircraftSymbolStyle : uint8_t {
    GullWing = 0,       ///< Classic yellow/amber twin L-wings with center bore-sight square
    ClassicW,           ///< W-shaped wing reference symbol with center apex
    RussianSilhouette   ///< KPPI winged silhouette with vertical stabilizer fin
};

/// Configuration options for the autonomous AttitudeView (Artificial Horizon / PFD) widget.
struct AttitudeViewOptions {
    // --- Element Companion Header / Footer & Outer Frame ---
    UiText label{};
    UiText description{};
    Icon icon{};
    UiText badge{};
    UiVariant variant{UiVariant::Default};
    ElementLayout layout{ElementLayout::Stacked};
    InlineOrder inlineOrder{InlineOrder::LabelFirst};
    Align align{Align::Start};
    bool background{false};
    float borderWidth{0.0f};
    std::optional<float> borderRadius{std::nullopt};

    // --- Inner Control Slot Configuration ---
    ControlSpec control{};

    // --- Display Mode, Reference Frame & Visual Theme ---
    AttitudeDisplayMode displayMode{AttitudeDisplayMode::FullPFD};
    HorizonReferenceMode referenceMode{HorizonReferenceMode::WesternMovingHorizon};
    AttitudeColorScheme colorScheme{AttitudeColorScheme::GlassCockpit};
    FlightDirectorStyle flightDirectorStyle{FlightDirectorStyle::SingleCueVBar};
    AircraftSymbolStyle aircraftSymbol{AircraftSymbolStyle::GullWing};

    // --- Primary Attitude & Air Data Telemetry ---
    float rollDeg{0.0f};                ///< Bank angle in degrees (-180 .. +180, positive = right wing down)
    float pitchDeg{0.0f};               ///< Pitch angle in degrees (-90 .. +90, positive = nose up)
    float headingDeg{0.0f};             ///< Magnetic/True heading in degrees [0 .. 360)
    float airspeedKnots{250.0f};        ///< Indicated Airspeed (IAS) in knots
    float machNumber{0.72f};            ///< Mach number readout below speed tape
    float altitudeFeet{12500.0f};       ///< Barometric altitude in feet
    float verticalSpeedFpm{0.0f};       ///< Vertical speed in feet per minute (-6000 .. +6000)
    float slipSkid{0.0f};               ///< Lateral inclinometer slip/skid ball offset (-1.0 .. +1.0)
    float gLoad{1.0f};                  ///< Normal load factor Nz in G
    float radioAltitudeFeet{2500.0f};   ///< Radio altimeter AGL height in feet (displayed when < 2500 ft)

    // --- Flight Path Vector (FPV), Flight Director (FD) & Autopilot Bugs ---
    bool showFlightPathVector{true};    ///< Render Flight Path Vector ("bird" / velocity vector reticle)
    float aoaDeg{2.2f};                 ///< Angle of Attack (alpha, shifts FPV vertically relative to pitch)
    float sideslipDeg{0.0f};            ///< Sideslip angle (beta, shifts FPV horizontally)
    float fdPitchDeg{2.5f};             ///< Flight Director commanded pitch in degrees
    float fdRollDeg{0.0f};              ///< Flight Director commanded roll in degrees
    float targetAirspeedBug{250.0f};    ///< Autopilot selected airspeed bug in knots
    float targetAltitudeBug{12500.0f};  ///< Autopilot selected altitude bug in feet
    float targetHeadingBug{0.0f};       ///< Autopilot selected heading bug in degrees
    float baroSettingInHg{29.92f};      ///< Barometric altimeter setting in inHg (e.g. 29.92)
    bool baroStd{false};                ///< Display "STD" barometric reference instead of inHg

    // --- ILS Approach Localizer & Glideslope Deviation ---
    bool showIlsDiamonds{true};         ///< Render ILS Localizer (bottom) and Glideslope (right) deviation scales
    float locDeviationDots{0.0f};       ///< Localizer lateral deviation in dots (-2.5 .. +2.5)
    float gsDeviationDots{0.0f};        ///< Glideslope vertical deviation in dots (-2.5 .. +2.5)

    // --- Symbology & Sub-Instrument Visibility Toggles ---
    bool showPitchLadder{true};         ///< Render 2.5/5/10 deg pitch ladder, zenith/nadir reticles, and chevrons
    bool showBankScale{true};           ///< Render upper bank arc (10/20/30/45/60 deg), roll pointer, and slip/skid ball
    bool showSpeedAndAltTapes{true};    ///< Render left Airspeed and right Altitude + VSI tapes in FullPFD mode
    bool showHeadingTape{true};         ///< Render magnetic compass heading tape
    bool showFmaBar{true};              ///< Render top Flight Mode Annunciator (FMA) status strip
    bool showMouseStickOverlay{false};  ///< Render interactive virtual-stick / mouse control reticle inside the ADI
    float fieldOfViewPitchDeg{50.0f};   ///< Vertical pitch span visible across the ADI diameter (30 .. 90 deg)

    // --- Optional Two-Way Bindings ---
    float* fovPitchDegPtr{nullptr};     ///< Optional external two-way binding for mouse-wheel pitch FOV zoom

    // --- Sizing & Styling ---
    StretchMode stretch{StretchMode::Fill};
    std::optional<float> width{std::nullopt};
    std::optional<float> height{std::nullopt};
    SizeMode heightMode{SizeMode::Stretch};
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
    std::string_view tooltip{};
};

/// Rich telemetry and cursor interaction state returned by `ui.AttitudeView(...)`.
struct AttitudeViewResponse : public Response {
    AttitudeDisplayMode displayMode{AttitudeDisplayMode::FullPFD};
    Rect adiRect{};                     ///< Resolved screen rectangle of the central artificial horizon sphere
    Vec2 adiCenter{};                   ///< Center point of the artificial horizon in screen coordinates
    float adiRadius{0.0f};              ///< Radius of the artificial horizon sphere in pixels
    float visibleFovPitchDeg{50.0f};    ///< Effective vertical pitch field-of-view in degrees

    bool cursorInside{false};           ///< True when mouse cursor is inside the widget control area
    bool cursorInsideAdi{false};        ///< True when mouse cursor is inside the ADI horizon region
    Vec2 cursorNorm{0.0f, 0.0f};        ///< Normalized cursor offset from ADI center in [-1.0, +1.0] (X = roll, Y = pitch)
    float cursorRollDeg{0.0f};          ///< Roll angle mapped from cursor X position (-65 .. +65 deg)
    float cursorPitchDeg{0.0f};         ///< Pitch angle mapped from cursor Y position (-40 .. +40 deg, up = nose up)
};

/// Autonomous Avionics Attitude Director Indicator (ADI) & Primary Flight Display (PFD) widget.
class AttitudeView {
public:
    static AttitudeViewResponse Run(
        Context& ctx,
        const AttitudeViewOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] static Vec2 Measure(
        const AttitudeViewOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        const UiTheme& theme = {},
        float availableWidth = 0.0f
    );
};

} // namespace guikit
