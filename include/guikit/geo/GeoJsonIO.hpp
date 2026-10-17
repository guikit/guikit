#pragma once

#include "guikit/geo/GeoTypes.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace guikit {

/// Result of parsing a GeoJSON document or string.
struct GeoJsonLoadResult {
    bool ok{false};
    std::string errorMessage;
    std::vector<GeoLayer> layers;
    GeoBBox totalBounds{};
    size_t totalFeatures{0};
    size_t totalVertices{0};
};

/// Fast RFC 7946 GeoJSON Reader, Writer, and Built-in Hydrographic / Maritime Chart Generator.
class GeoJsonIO {
public:
    /// Parses a GeoJSON string (`FeatureCollection`, `Feature`, or raw `Geometry`).
    /// Automatically groups features into thematic layers by S-57 `OBJL` / `layer` property if present,
    /// or places them into `defaultLayerName`.
    [[nodiscard]] static GeoJsonLoadResult ParseFromString(
        std::string_view jsonText,
        std::string_view defaultLayerName = "GeoJSON Layer",
        bool splitByThematicClass = true
    );

    /// Loads and parses a `.geojson` or `.json` file from embedded assets (`guikit::Assets`) or disk.
    [[nodiscard]] static GeoJsonLoadResult LoadFromFile(
        const std::string& filePath,
        bool splitByThematicClass = true
    );

    /// Serializes one or more `GeoLayer`s into an RFC 7946 `FeatureCollection` GeoJSON string.
    [[nodiscard]] static std::string SerializeToString(
        const std::vector<GeoLayer>& layers,
        bool prettyPrint = true
    );

    /// Saves one or more `GeoLayer`s to a `.geojson` file on disk.
    [[nodiscard]] static bool SaveToFile(
        const std::string& filePath,
        const std::vector<GeoLayer>& layers,
        bool prettyPrint = true
    );

    /// Builds a rich multi-layer hydrographic ENC / Navionics chart dataset (Bathymetry DEPARE,
    /// Isobaths DEPCNT, Land LNDARE, Coastline COALNE, Soundings SOUNDG, IALA Seamarks, Fairways,
    /// Restricted Areas, Submarine Cables, and Underwater Hazards) for immediate GIS inspection.
    [[nodiscard]] static std::vector<GeoLayer> BuildDefaultHydrographicChart();
};

} // namespace guikit
