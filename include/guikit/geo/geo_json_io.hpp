#pragma once

#include <cstddef>
#include "guikit/geo/geo_types.hpp"
#include "keel/array.h"
#include "keel/ref.h"
#include "keel/string.h"

namespace guikit::geo {

/// Result of parsing a GeoJSON document into a single vector GIS layer.
struct GeoJsonResult {
    bool ok{false};
    keel::String error{};
    GeoLayer layer{};

    [[nodiscard]] keel::StringRef Error() const noexcept { return error.as_ref(); }
};

/// Result of parsing a GeoJSON document into one or more thematic vector GIS layers.
struct GeoJsonLoadResult {
    bool ok{false};
    keel::String errorMessage{};
    keel::Array<GeoLayer> layers{};
    guikit::GeoBounds totalBounds{};
    bool hasValidBounds{false};
    size_t totalFeatures{0};
    size_t totalVertices{0};

    [[nodiscard]] keel::StringRef Error() const noexcept { return errorMessage.as_ref(); }
};

/// RFC 7946 GeoJSON Parser and Serializer (supports both disk files and embedded `guikit::Assets`).
class GeoJsonIO {
public:
    /// Parses a GeoJSON document from an in-memory UTF-8 string into a single `GeoLayer`.
    [[nodiscard]] static GeoJsonResult ParseFromString(
        keel::StringRef jsonText,
        keel::StringRef defaultLayerName = "GeoJSON Layer"
    );

    /// Loads a GeoJSON file (`.geojson` / `.json`) from embedded `guikit::Assets` or disk into a single `GeoLayer`.
    [[nodiscard]] static GeoJsonResult LoadFromFile(keel::StringRef filePath);

    /// Parses a GeoJSON string and optionally splits features into thematic S-57 `GeoLayer`s.
    [[nodiscard]] static GeoJsonLoadResult ParseThematicLayersFromString(
        keel::StringRef jsonText,
        keel::StringRef defaultLayerName,
        bool splitByThematicClass
    );

    /// Loads a GeoJSON file from embedded `guikit::Assets` or disk into thematic S-57 `GeoLayer`s.
    [[nodiscard]] static GeoJsonLoadResult LoadThematicLayersFromFile(
        keel::StringRef filePath,
        bool splitByThematicClass = true
    );

    /// Two-argument overload returning `GeoJsonLoadResult` with thematic layer splitting.
    [[nodiscard]] static GeoJsonLoadResult LoadFromFile(
        keel::StringRef filePath,
        bool splitByThematicClass
    ) {
        return LoadThematicLayersFromFile(filePath, splitByThematicClass);
    }

    /// Serializes a `GeoLayer` to an RFC 7946 compliant GeoJSON FeatureCollection string.
    [[nodiscard]] static keel::String SerializeLayer(const GeoLayer& layer, bool prettyPrint = true);

    /// Writes a `GeoLayer` to disk as a `.geojson` file.
    [[nodiscard]] static bool SaveToFile(
        const GeoLayer& layer,
        keel::StringRef filePath,
        keel::String* outError = nullptr
    );

    /// Loads the built-in embedded hydrographic ENC / S-57 chart (`assets/geo/demo_bay.geojson`).
    [[nodiscard]] static keel::Array<GeoLayer> BuildDefaultHydrographicChart();
};

} // namespace guikit::geo

namespace guikit {
using geo::GeoJsonIO;
using geo::GeoJsonLoadResult;
using geo::GeoJsonResult;
} // namespace guikit
