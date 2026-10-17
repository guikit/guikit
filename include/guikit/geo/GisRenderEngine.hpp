#pragma once

#include "guikit/geo/GeoTypes.hpp"
#include "guikit/panels/MapView.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace guikit {

/// Hydrographic / GIS Chart Color Scheme (S-52 / Navionics style).
enum class GisColorPalette : uint8_t {
    NavionicsDay = 0,   ///< Bright high-contrast marine chart (warm land, 4-zone blue bathymetry)
    NavionicsDusk = 1,  ///< Twilight marine chart (muted amber land, deep navy bathymetry)
    NavionicsNight = 2, ///< Bridge night-vision mode (dark red/slate land, black-blue water)
    ClassicGis = 3      ///< Topographic / Vector GIS palette
};

[[nodiscard]] constexpr std::string_view GisColorPaletteName(GisColorPalette p) noexcept {
    switch (p) {
        case GisColorPalette::NavionicsDay:   return "Navionics Day (S-52 Bright)";
        case GisColorPalette::NavionicsDusk:  return "Navionics Dusk (Twilight)";
        case GisColorPalette::NavionicsNight: return "Navionics Night (Bridge Dark)";
        case GisColorPalette::ClassicGis:     return "Classic Vector GIS";
    }
    return "Navionics Day";
}

/// Interactive rendering & filtering settings for the GIS chart viewport.
struct GisRenderSettings {
    GisColorPalette palette{GisColorPalette::NavionicsDay};
    float safetyContourMeters{5.0f};   ///< S-52 Safety Depth Contour (highlights shallower water)
    float shallowContourMeters{2.0f};  ///< Very shallow water threshold
    float deepContourMeters{10.0f};    ///< Deep water threshold

    bool showFilledPolygons{true};     ///< Render triangulated polygon fills (LNDARE / DEPARE)
    bool showPolygonOutlines{true};    ///< Render polygon boundary strokes
    bool showDepthContours{true};      ///< Render bathymetric isobaths (DEPCNT)
    bool showSoundings{true};          ///< Render spot soundings (SOUNDG) with subscript tenths
    bool showSeamarks{true};           ///< Render IALA buoys, beacons, and light sectors
    bool showFeatureLabels{true};      ///< Render feature names and contour depth labels
    bool showVerticesOnSelect{true};   ///< Show vertex handles on selected geometry
    bool highlightSafetyContour{true}; ///< Bold highlight on contours <= safetyContourMeters
};

/// Result of per-frame GIS vector rendering & spatial hit-testing.
struct GisRenderStats {
    size_t layersRendered{0};
    size_t featuresTotal{0};
    size_t featuresCulled{0};
    size_t featuresRendered{0};
    size_t trianglesSubmitted{0};
    size_t segmentsSubmitted{0};
    size_t pointsSubmitted{0};

    std::optional<uint64_t> hoveredFeatureId;
    std::optional<uint32_t> hoveredLayerId;
    std::optional<uint64_t> clickedFeatureId;
    std::optional<uint32_t> clickedLayerId;
};

/// Renders thematic `GeoLayer`s onto a `guikit::MapViewScope` with frustum culling,
/// S-52 / Navionics bathymetric shading, spot sounding typography, and spatial hit-testing.
class GisRenderEngine {
public:
    static GisRenderStats RenderLayers(
        MapViewScope& map,
        std::span<const GeoLayer> layers,
        const GisRenderSettings& settings,
        std::optional<uint64_t> selectedFeatureId,
        Vec2 mouseScreenPos,
        bool mouseClickedInMap
    );

    /// Computes the background water color for the active palette.
    [[nodiscard]] static Color GetWaterBackgroundColor(GisColorPalette palette) noexcept;
};

} // namespace guikit
