#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "guikit/geo/GeoTypes.hpp"

namespace guikit::geo {

/// Result of parsing a GeoJSON document into a single vector GIS layer.
struct GeoJsonResult {
    bool ok{false};
    std::string error{};
    GeoLayer layer{};
};

/// Result of parsing a GeoJSON document into one or more thematic vector GIS layers.
struct GeoJsonLoadResult {
    bool ok{false};
    std::string errorMessage{};
    std::vector<GeoLayer> layers{};
    guikit::GeoBounds totalBounds{};
    bool hasValidBounds{false};
    size_t totalFeatures{0};
    size_t totalVertices{0};
};

/// RFC 7946 GeoJSON Parser and Serializer (supports both disk files and embedded `guikit::Assets`).
class GeoJsonIO {
public:
    /// Parses a GeoJSON document from an in-memory UTF-8 string into a single `GeoLayer`.
    [[nodiscard]] static GeoJsonResult ParseFromString(
        std::string_view jsonText,
        std::string_view defaultLayerName = "GeoJSON Layer"
    );

    /// Loads a GeoJSON file (`.geojson` / `.json`) from embedded `guikit::Assets` or disk into a single `GeoLayer`.
    [[nodiscard]] static GeoJsonResult LoadFromFile(const std::string& filePath);

    /// Parses a GeoJSON string and optionally splits features into thematic S-57 `GeoLayer`s.
    [[nodiscard]] static GeoJsonLoadResult ParseThematicLayersFromString(
        std::string_view jsonText,
        std::string_view defaultLayerName,
        bool splitByThematicClass
    );

    /// Loads a GeoJSON file from embedded `guikit::Assets` or disk into thematic S-57 `GeoLayer`s.
    [[nodiscard]] static GeoJsonLoadResult LoadThematicLayersFromFile(
        const std::string& filePath,
        bool splitByThematicClass = true
    );

    /// Two-argument overload returning `GeoJsonLoadResult` with thematic layer splitting.
    [[nodiscard]] static GeoJsonLoadResult LoadFromFile(
        const std::string& filePath,
        bool splitByThematicClass
    ) {
        return LoadThematicLayersFromFile(filePath, splitByThematicClass);
    }

    /// Serializes a `GeoLayer` to an RFC 7946 compliant GeoJSON FeatureCollection string.
    [[nodiscard]] static std::string SerializeLayer(const GeoLayer& layer, bool prettyPrint = true);

    /// Writes a `GeoLayer` to disk as a `.geojson` file.
    [[nodiscard]] static bool SaveToFile(
        const GeoLayer& layer,
        const std::string& filePath,
        std::string* outError = nullptr
    );

    /// Loads the built-in embedded hydrographic ENC / S-57 chart (`assets/geo/demo_bay.geojson`).
    [[nodiscard]] static std::vector<GeoLayer> BuildDefaultHydrographicChart();
};

} // namespace guikit::geo

namespace guikit {
using namespace geo;
} // namespace guikit
