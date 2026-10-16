#pragma once

#include <array>
#include <string_view>
#include "guikit/panels/MapView.hpp"

namespace guikit::demo {

struct MaritimeWaypoint {
    std::string_view id;
    std::string_view name;
    std::string_view seamarkType;
    GeoCoord coord;
    float defaultTileZoom;
};

inline constexpr std::array<MaritimeWaypoint, 5> kMaritimeWaypoints = {{
    {"WP-HELGOLAND", "Helgoland Light (North Sea)",   "IALA Major Lighthouse (Fl 5s 28M)",    {54.1825,  7.8853}, 9.0f},
    {"WP-KIEL",      "Brunsbüttel / Kiel Canal",      "Locks & VTS Reporting Point",          {53.8900,  9.1433}, 9.5f},
    {"WP-DOVER",     "Dover Strait TSS",              "Traffic Separation Buoy (Safe Water)", {51.1133,  1.3400}, 8.5f},
    {"WP-GREENWICH", "Greenwich Prime Meridian",      "000° 00.00' E Geodetic Reference",     {51.4779,  0.0000}, 9.0f},
    {"WP-GIBRALTAR", "Strait of Gibraltar (Europa)",  "Cardinal Pillar & Sector Light",       {36.1095, -5.3453}, 8.5f}
}};

/// Renders the demo IALA nautical seamarks and rhumb-line shipping courses onto a MapViewScope.
inline void RenderDemoMaritimeOverlay(MapViewScope& map, bool lightMode) {
    const Color seamarkMagenta = lightMode ? Color::Rgba(185, 28, 140, 185) : Color::Rgba(232, 121, 249, 185);
    const Color seamarkCyan    = lightMode ? Color::Rgba(2, 132, 199, 175)  : Color::Rgba(56, 189, 248, 175);

    // Rhumb-line (loxodrome) shipping lanes connecting Kiel -> Helgoland -> Dover -> Gibraltar
    map.DrawGeoLine(kMaritimeWaypoints[1].coord, kMaritimeWaypoints[0].coord, seamarkMagenta, 1.8f);
    map.DrawGeoLine(kMaritimeWaypoints[0].coord, kMaritimeWaypoints[2].coord, seamarkMagenta, 1.8f);
    map.DrawGeoLine(kMaritimeWaypoints[2].coord, kMaritimeWaypoints[3].coord, seamarkCyan,    1.5f);
    map.DrawGeoLine(kMaritimeWaypoints[2].coord, kMaritimeWaypoints[4].coord, seamarkMagenta, 1.8f);

    for (const MaritimeWaypoint& wp : kMaritimeWaypoints) {
        map.DrawSeamark(wp.coord, {
            .id = wp.id,
            .title = wp.name,
            .category = wp.seamarkType
        });
    }
}

} // namespace guikit::demo
